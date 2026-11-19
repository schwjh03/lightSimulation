#ifndef _WIN32
#include "app.hpp"
#include "detector_windows.hpp"
#include "raymath.h"
#define Font X11Font
#define Window X11Window
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef Window
#undef Font
#include <algorithm>
#include <cmath>
#include <map>
#include <fstream>
#include <vector>

namespace lightlab {
namespace {
enum class Kind { Detector, Prism, Splitter, Mirror, Wall, Laser };

struct Widget {
    int y, action;
    float minimum, maximum;
    int x = 20, width = 340;
};

struct Meter {
    X11Window window{};
    std::uint64_t id{};
    int number{};
    DetectorReading reading;
    App* app{};
    Kind kind{};
    Prism prism;
    Surface surface;
    Emitter emitter;
    bool reflections{};
    std::vector<Widget> widgets;
    Widget activeSlider{};

    void dragSlider(int x) {
        if (activeSlider.action)
            apply(activeSlider.action, activeSlider.minimum + std::clamp((x - 20) / 340.f, 0.f, 1.f) *
                                                                  (activeSlider.maximum - activeSlider.minimum));
    }

    bool sync(App& target) {
        app = &target;
        for (size_t i = 0; i < app->scene.surfaces.size(); ++i) {
            const auto& value = app->scene.surfaces[i];
            if (value.id != id)
                continue;
            surface = value;
            number = static_cast<int>(i + 1);
            kind = value.material == Material::Detector   ? Kind::Detector
                   : value.material == Material::Splitter ? Kind::Splitter
                   : value.material == Material::Mirror   ? Kind::Mirror
                                                          : Kind::Wall;
            reading = i < app->readings.size() ? app->readings[i] : DetectorReading{};
            return true;
        }
        for (size_t i = 0; i < app->scene.prisms.size(); ++i)
            if (app->scene.prisms[i].id == id) {
                prism = app->scene.prisms[i];
                kind = Kind::Prism;
                number = static_cast<int>(i + 1);
                reflections = app->scene.partialReflections;
                return true;
            }
        for (size_t i = 0; i < app->scene.lasers.size(); ++i)
            if (app->scene.lasers[i].id == id) {
                emitter = app->scene.lasers[i];
                kind = Kind::Laser;
                number = static_cast<int>(i + 1);
                return true;
            }
        return false;
    }

    void apply(int action, float value) {
        if (!app)
            return;
        if (isLengthControl(action))
            value = fromCentimeters(value);
        if (action == 35) {
            app->eraseObject(id);
            return;
        }
        for (auto& p : app->scene.prisms)
            if (p.id == id) {
                if (action == 33)
                    p.flipped = !p.flipped;
                if (action == 1)
                    p.apex = value;
                if (action == 2)
                    p.angle = value;
                if (action == 3)
                    p.radius = value;
                if (action == 36) {
                    p.thickness = lensThickness(p);
                    p.radius = value / 2;
                }
                if (action == 37)
                    p.thickness = value;
                if (action >= 4 && action <= 6) {
                    p.glass = static_cast<Glass>(action - 4);
                    p.customIndex = 0;
                }
                if (action == 34)
                    p.customIndex = std::clamp(value, 1.f, 2.5f);
                if (action == 7)
                    app->scene.partialReflections = !app->scene.partialReflections;
                p = fitPrism(p);
            }
        for (auto& item : app->scene.surfaces)
            if (item.id == id) {
                if (action == 8)
                    item.reflectivity = value / 100;
                if (action >= 9 && action <= 11)
                    item.reflectivity = .25f + (action - 9) * .25f;
                if (action == 12 || action == 13) {
                    auto center = Vector2Scale(Vector2Add(item.start, item.end), .5f);
                    float angle = action == 13 ? value * DEG2RAD
                                               : std::atan2(item.end.y - item.start.y, item.end.x - item.start.x);
                    float extent = action == 12 ? value : surfaceLength(item);
                    float maxX = (worldWidth - 24) / std::max(.001f, std::abs(std::cos(angle)));
                    float maxY = (worldHeight - 24) / std::max(.001f, std::abs(std::sin(angle)));
                    extent = std::clamp(extent, 8.f, std::min(maxX, maxY));
                    Vector2 half{std::cos(angle) * extent / 2, std::sin(angle) * extent / 2};
                    item.start = Vector2Subtract(center, half);
                    item.end = Vector2Add(center, half);
                    Vector2 lo = Vector2Min(item.start, item.end), hi = Vector2Max(item.start, item.end);
                    Vector2 shift{std::clamp(0.f, 12 - lo.x, worldWidth - 12 - hi.x),
                                  std::clamp(0.f, 12 - lo.y, worldHeight - 12 - hi.y)};
                    item.start = Vector2Add(item.start, shift);
                    item.end = Vector2Add(item.end, shift);
                }
            }
        for (auto& light : app->scene.lasers)
            if (light.id == id) {
                if (action == 14)
                    light.angle = value;
                if (action == 15)
                    light.spread = value;
                if (action == 16)
                    light.rayCount = static_cast<int>(std::round(value));
                if (action == 17)
                    light.bounces = static_cast<int>(std::round(value));
                if (action == 32)
                    light.sweep = !light.sweep;
                if (action >= 20 && action <= 26)
                    light.colorIndex = action - 20;
                if (action == 30)
                    light.mode = EmitterMode::Beam;
                if (action == 31)
                    light.mode = EmitterMode::Point;
                if (action == 38) {
                    light.mode = EmitterMode::Wall;
                    if (light.rayCount < 2)
                        light.rayCount = 9;
                }
                if (action == 39)
                    light.width = value;
                light = fitEmitter(light);
            }
        sync(*app);
    }
};
}

struct DetectorWindows::Impl {
    Display* display = XOpenDisplay(nullptr);
    Atom close = display ? XInternAtom(display, "WM_DELETE_WINDOW", False) : 0;
    bool hidden;
    std::map<std::uint64_t, Meter> meters;

    explicit Impl(bool hidden) : hidden(hidden) {}

    ~Impl() {
        if (display)
            XCloseDisplay(display);
    }

    void draw(Meter& m, Drawable target = 0) {
        XWindowAttributes bounds{};
        if (!XGetWindowAttributes(display, m.window, &bounds) || bounds.width <= 0 || bounds.height <= 0)
            return;
        // Present one completed frame. Drawing directly into the window lets the
        // compositor show the background clear between individual text/shape requests.
        const bool present = target == 0;
        if (present)
            target = XCreatePixmap(display, m.window, bounds.width, bounds.height, bounds.depth);
        GC gc = XCreateGC(display, target, 0, nullptr);
        XSetGraphicsExposures(display, gc, False);
        XSetForeground(display, gc, 0x121a27);
        XFillRectangle(display, target, gc, 0, 0, bounds.width, bounds.height);
        auto text = [&](int x, int y, const std::string& label) {
            XSetForeground(display, gc, 0xe4ecf7);
            XDrawString(display, target, gc, x, y, label.c_str(), label.size());
        };
        text(20, 30, "Light Lab 2D - Object " + std::to_string(m.number));
        int y = 65;
        m.widgets.clear();
        int deleteX = bounds.width - 90;
        XSetForeground(display, gc, 0xa32d40);
        XFillRectangle(display, target, gc, deleteX, 12, 70, 26);
        XSetForeground(display, gc, 0xf27080);
        XDrawRectangle(display, target, gc, deleteX, 12, 70, 26);
        text(deleteX + 14, 29, "Delete");
        m.widgets.push_back({12, 35, 0, 0, deleteX, 70});
        auto control = [&](const std::string& label, int action, float value, float low, float high) {
            const bool length = isLengthControl(action);
            if (length) {
                value = toCentimeters(value);
                low = toCentimeters(low);
                high = toCentimeters(high);
            }
            std::string formatted = length                   ? TextFormat("%.2f cm", value)
                                    : action == 34           ? TextFormat("%.3f", value)
                                    : isAngleControl(action) ? TextFormat("%.1f deg", value)
                                                             : std::to_string(static_cast<int>(value));
            text(20, y, label + ": " + formatted);
            XSetForeground(display, gc, 0x2b3a4e);
            XFillRectangle(display, target, gc, 20, y + 8, 340, 14);
            XSetForeground(display, gc, 0x1990a2);
            XFillRectangle(display, target, gc, 20, y + 8,
                           static_cast<unsigned>(340 * std::clamp((value - low) / (high - low), 0.f, 1.f)), 14);
            m.widgets.push_back({y + 8, action, low, high});
            y += 52;
        };
        auto button = [&](const std::string& label, int action, bool chosen = false) {
            XSetForeground(display, gc, chosen ? 0x176b78 : 0x1f2e41);
            XFillRectangle(display, target, gc, 20, y, 340, 20);
            XSetForeground(display, gc, chosen ? 0x58e8c7 : 0x52667f);
            XDrawRectangle(display, target, gc, 20, y, 340, 20);
            text(30, y + 14, label);
            m.widgets.push_back({y, action, 0, 0});
            y += 30;
        };
        if (m.kind == Kind::Laser) {
            control("Direction", 14, m.emitter.angle, -180, 180);
            if (m.emitter.mode == EmitterMode::Wall)
                control("Width", 39, m.emitter.width, 8, 400);
            else
                control("Spread", 15, m.emitter.spread, 0, 120);
            control("Rays", 16, m.emitter.rayCount, m.emitter.mode == EmitterMode::Wall ? 2 : 1, 240);
            control("Reflections", 17, m.emitter.bounces, 0, 12);
            button(m.emitter.sweep ? "Sweep: on" : "Sweep: off", 32);
            button("Beam mode", 30);
            button("Point mode", 31);
            button("Laser wall mode", 38);
            for (int i = 0; i < 7; ++i)
                button(colorNames[i], 20 + i);
        } else if (m.kind == Kind::Prism) {
            if (m.prism.lens == LensShape::Prism)
                control("Apex", 1, m.prism.apex, 20, 100);
            else {
                text(20, y, lensName(m.prism.lens));
                y += 30;
            }
            control("Rotation", 2, m.prism.angle, -180, 180);
            if (m.prism.lens == LensShape::Prism)
                control("Radius", 3, m.prism.radius, 24, 210);
            else {
                control("Length", 36, 2 * m.prism.radius, 48, 420);
                control("Thickness (center)", 37, lensThickness(m.prism), minimumLensThickness(m.prism), 300);
            }
            y += 12;
            for (int i = 0; i < 3; ++i) {
                std::string label = std::string(i == 0   ? "Flint"
                                                : i == 1 ? "Crown"
                                                         : "Water") +
                                    TextFormat(" (n=%.3f)", glassIndex(589, static_cast<Glass>(i)));
                button(label, 4 + i, m.prism.customIndex == 0 && m.prism.glass == static_cast<Glass>(i));
                y += 8;
            }
            y += 16;
            control("n (589 nm)", 34, objectIndex(589, m.prism), 1, 2.5f);
            y += 12;
            button(m.reflections ? "Fresnel reflections (all glass): on" : "Fresnel reflections (all glass): off", 7,
                   m.reflections);
            if (m.prism.lens == LensShape::PlanoConvex || m.prism.lens == LensShape::PlanoConcave ||
                m.prism.lens == LensShape::PositiveMeniscus || m.prism.lens == LensShape::NegativeMeniscus)
                button("Flip", 33, m.prism.flipped);
        } else {
            control("Length", 12, surfaceLength(m.surface), 8, maximumSurfaceLength());
            control("Rotation", 13,
                    std::atan2(m.surface.end.y - m.surface.start.y, m.surface.end.x - m.surface.start.x) * RAD2DEG,
                    -180, 180);
            if (m.kind == Kind::Splitter)
                control("Reflected percent", 8, m.surface.reflectivity * 100, 0, 100);

            if (m.kind == Kind::Mirror) {
                text(20, y, "Mirror reflectance: 82% (fixed)");
                y += 22;
            }
            if (m.kind == Kind::Detector) {
                text(20, y, TextFormat("Received: %.2f%% of total emitted power", m.reading.power * 100));
                y += 30;
                for (auto band : m.reading.spectrum) {
                    text(20, y, std::to_string(static_cast<int>(band.wavelength)) + " nm");
                    XSetForeground(display, gc, 0x58e8c7);
                    XFillRectangle(display, target, gc, 90, y - 10,
                                   static_cast<unsigned>(240 * std::clamp(band.weight, 0.f, 1.f)), 10);
                    y += 22;
                }
            }
        }
        if (m.kind == Kind::Prism) {
            text(20, y + 18,
                 m.prism.customIndex != 0        ? "Custom n: shifted material dispersion"
                 : m.prism.glass == Glass::Water ? "Water: approximate Cauchy model"
                                                 : "Glass: catalog Sellmeier model");
        }
        if (m.kind == Kind::Prism && m.prism.lens != LensShape::Prism) {
            auto radii = lensRadiiCm(m.prism);
            std::string r1 = std::isinf(radii[0]) ? "flat" : TextFormat("%.2f cm", radii[0]);
            std::string r2 = std::isinf(radii[1]) ? "flat" : TextFormat("%.2f cm", radii[1]);
            text(20, y + 38, "R1: " + r1 + " / R2: " + r2);
            float focal = paraxialFocalLengthCm(m.prism);
            text(20, y + 58,
                 std::isinf(focal) ? "Paraxial f (589 nm): infinity"
                                   : TextFormat("Paraxial f (589 nm): %+.2f cm", focal));
        }
        if (present) {
            XCopyArea(display, target, m.window, gc, 0, 0, bounds.width, bounds.height, 0, 0);
            XFreePixmap(display, target);
        }
        XFreeGC(display, gc);
    }
};

DetectorWindows::DetectorWindows(void*, bool hidden) : impl(std::make_unique<Impl>(hidden)) {}

DetectorWindows::~DetectorWindows() {
    closeAll();
}

void DetectorWindows::closeAll() {
    if (impl->display)
        for (auto& pair : impl->meters)
            XDestroyWindow(impl->display, pair.second.window);
    impl->meters.clear();
}

void DetectorWindows::update(App& app) {
    app.ensureSurfaceIds();
    if (!impl->display) {
        app.detectorWindowRequests.clear();
        return;
    }
    for (auto id : app.detectorWindowRequests) {
        if (impl->meters.count(id)) {
            XRaiseWindow(impl->display, impl->meters.at(id).window);
            continue;
        }
        Meter m;
        m.id = id;
        if (!m.sync(app))
            continue;
        m.window =
            XCreateSimpleWindow(impl->display, DefaultRootWindow(impl->display), 80, 80, 390, 700, 0, 0, 0x121a27);
        XStoreName(impl->display, m.window, "Light Lab 2D - Inspector");
        XSetWMProtocols(impl->display, m.window, &impl->close, 1);
        XSelectInput(impl->display, m.window, ExposureMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
        if (!impl->hidden)
            XMapWindow(impl->display, m.window);
        impl->meters.emplace(id, std::move(m));
    }
    app.detectorWindowRequests.clear();
    while (XPending(impl->display)) {
        XEvent event;
        XNextEvent(impl->display, &event);
        for (auto it = impl->meters.begin(); it != impl->meters.end(); ++it) {
            auto& m = it->second;
            if (m.window != event.xany.window)
                continue;
            if (event.type == ClientMessage && static_cast<Atom>(event.xclient.data.l[0]) == impl->close) {
                XDestroyWindow(impl->display, m.window);
                impl->meters.erase(it);
                break;
            }
            if (event.type == ButtonPress && event.xbutton.button == Button1) {
                for (auto w : m.widgets)
                    if (event.xbutton.y >= w.y && event.xbutton.y <= w.y + (w.action == 35 ? 26 : 20) &&
                        event.xbutton.x >= w.x && event.xbutton.x <= w.x + w.width) {
                        if (w.action != 35)
                            app.checkpoint();
                        if (w.maximum > w.minimum)
                            m.activeSlider = w;
                        m.apply(w.action, w.minimum + std::clamp((event.xbutton.x - 20) / 340.f, 0.f, 1.f) *
                                                          (w.maximum - w.minimum));
                        break;
                    }
            }
            if (event.type == MotionNotify && (event.xmotion.state & Button1Mask))
                m.dragSlider(event.xmotion.x);
            if (event.type == ButtonRelease && event.xbutton.button == Button1) {
                m.dragSlider(event.xbutton.x);
                m.activeSlider = {};
            }
            break;
        }
    }
    for (auto it = impl->meters.begin(); it != impl->meters.end();) {
        if (!it->second.sync(app)) {
            XDestroyWindow(impl->display, it->second.window);
            it = impl->meters.erase(it);
        } else {
            impl->draw(it->second);
            ++it;
        }
    }
    XFlush(impl->display);
}

std::size_t DetectorWindows::count() const {
    return impl->meters.size();
}

void* DetectorWindows::nativeHandle(std::uint64_t id) const {
    auto it = impl->meters.find(id);
    return it == impl->meters.end() ? nullptr : reinterpret_cast<void*>(it->second.window);
}

float DetectorWindows::displayedPower(std::uint64_t id) const {
    auto it = impl->meters.find(id);
    return it == impl->meters.end() ? 0 : it->second.reading.power;
}

bool DetectorWindows::captureFirst(const std::string& path) const {
    return captureWindow(0, path);
}

bool DetectorWindows::captureWindow(std::uint64_t id, const std::string& path) const {
    for (auto& pair : impl->meters) {
        if (id && pair.first != id)
            continue;
        XWindowAttributes attrs{};
        XGetWindowAttributes(impl->display, pair.second.window, &attrs);
        Pixmap pixmap = XCreatePixmap(impl->display, pair.second.window, attrs.width, attrs.height, attrs.depth);
        impl->draw(pair.second, pixmap);
        XImage* pixels = XGetImage(impl->display, pixmap, 0, 0, attrs.width, attrs.height, AllPlanes, ZPixmap);
        XFreePixmap(impl->display, pixmap);
        if (!pixels)
            return false;
        Image image = GenImageColor(attrs.width, attrs.height, BLACK);
        auto channel = [](unsigned long pixel, unsigned long mask) {
            if (!mask)
                return static_cast<unsigned char>(0);
            while (!(mask & 1)) {
                mask >>= 1;
                pixel >>= 1;
            }
            return static_cast<unsigned char>((pixel & mask) * 255 / mask);
        };
        auto* data = static_cast<Color*>(image.data);
        for (int y = 0; y < attrs.height; ++y)
            for (int x = 0; x < attrs.width; ++x) {
                auto pixel = XGetPixel(pixels, x, y);
                data[y * attrs.width + x] = {channel(pixel, pixels->red_mask), channel(pixel, pixels->green_mask),
                                             channel(pixel, pixels->blue_mask), 255};
            }
        XDestroyImage(pixels);
        // Write BMP directly: installed raylib builds may disable BMP export.
        std::ofstream file(path, std::ios::binary);
        auto word = [&](std::uint32_t value, int bytes) {
            for (int i = 0; i < bytes; ++i)
                file.put(static_cast<char>(value >> (8 * i)));
        };
        file.write("BM", 2);
        word(54 + attrs.width * attrs.height * 4, 4);
        word(0, 4);
        word(54, 4);
        word(40, 4);
        word(attrs.width, 4);
        word(attrs.height, 4);
        word(1, 2);
        word(32, 2);
        word(0, 4);
        word(attrs.width * attrs.height * 4, 4);
        for (int i = 0; i < 4; ++i)
            word(0, 4);
        for (int y = attrs.height - 1; y >= 0; --y)
            for (int x = 0; x < attrs.width; ++x) {
                auto pixel = data[y * attrs.width + x];
                file.put(pixel.b);
                file.put(pixel.g);
                file.put(pixel.r);
                file.put(pixel.a);
            }
        file.close();
        bool saved = static_cast<bool>(file);
        UnloadImage(image);
        return saved;
    }
    return false;
}
} // namespace lightlab
#endif
