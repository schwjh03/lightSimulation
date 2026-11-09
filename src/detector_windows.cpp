#include "native_windows.hpp"
#include "detector_windows.hpp"
#include "app.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <map>
#include <vector>

namespace lightlab {
namespace {
constexpr wchar_t meterClass[] = L"LightLab2D.DetectorMeter";

COLORREF rgb(Color color) {
    return RGB(color.r, color.g, color.b);
}

enum class Kind { Detector, Prism, Splitter, Mirror, Wall, Laser };

struct Widget {
    RECT rect;
    int action;
    bool slider;
    float minimum, maximum;
};

struct Meter {
    HWND window = nullptr;
    std::uint64_t id = 0;
    int number = 0;
    DetectorReading reading;
    App* app = nullptr;
    Kind kind = Kind::Detector;
    Prism prism;
    Surface surface;
    Emitter emitter;
    bool reflections = true;
    std::vector<Widget> widgets;
    int activeAction = 0;

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

    std::wstring label() const {
        switch (kind) {
        case Kind::Prism:
            return L"Glass prism";
        case Kind::Splitter:
            return L"Beam splitter";
        case Kind::Mirror:
            return L"Mirror";
        case Kind::Wall:
            return L"Wall";
        case Kind::Laser:
            return L"Laser source";
        default:
            return L"Light detector";
        }
    }

    std::wstring title() const { return label() + L" " + std::to_wstring(number) + L" - Light Lab 2D"; }

    void apply(int action, float value) {
        if (!app)
            return;
        for (auto& p : app->scene.prisms)
            if (p.id == id) {
                if (action == 1)
                    p.apex = value;
                if (action == 2)
                    p.angle = value;
                if (action == 3)
                    p.radius = value;
                if (action >= 4 && action <= 6)
                    p.glass = static_cast<Glass>(action - 4);
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
            }
        sync(*app);
        InvalidateRect(window, nullptr, FALSE);
    }

    void adjust(POINT point) {
        for (auto widget : widgets)
            if (widget.action == activeAction) {
                float fraction = std::clamp((point.x - widget.rect.left) /
                                                static_cast<float>(std::max(1L, widget.rect.right - widget.rect.left)),
                                            0.f, 1.f);
                apply(widget.action, widget.minimum + fraction * (widget.maximum - widget.minimum));
                break;
            }
    }

    HFONT fonts[4]{};
    UINT fontDpi = 0;
    RECT chart{};
    RECT lastBounds{};
    int hoverBand = -1;

    ~Meter() {
        for (auto font : fonts)
            if (font)
                DeleteObject(font);
    }

    void prepareFonts(UINT dpi) {
        if (fontDpi == dpi)
            return;
        const int sizes[] = {12, 15, 24, 42};
        for (int i = 0; i < 4; ++i) {
            if (fonts[i])
                DeleteObject(fonts[i]);
            fonts[i] = CreateFontW(-MulDiv(sizes[i], dpi, 96), 0, 0, 0, i >= 2 ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE,
                                   FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH, L"Segoe UI");
        }
        fontDpi = dpi;
    }

    void draw(HDC dc, int width, int height) {
        UINT dpi = window ? GetDpiForWindow(window) : 96;
        if (!dpi)
            dpi = 96;
        prepareFonts(dpi);
        float scale = dpi / 96.f;
        auto px = [&](float value) { return static_cast<int>(std::round(value * scale)); };
        const float w = width / scale, h = height / scale;
        auto fill = [&](float x, float y, float rw, float rh, COLORREF color) {
            RECT rect{px(x), px(y), px(x + rw), px(y + rh)};
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        };
        auto text = [&](const std::wstring& value, float x, float y, int font, COLORREF color) {
            auto old = SelectObject(dc, fonts[font]);
            SetTextColor(dc, color);
            TextOutW(dc, px(x), px(y), value.c_str(), static_cast<int>(value.size()));
            SelectObject(dc, old);
        };
        widgets.clear();
        SetBkMode(dc, TRANSPARENT);
        fill(0, 0, w, h, RGB(18, 28, 43));
        if (kind != Kind::Detector) {
            Color accent = kind == Kind::Prism      ? prismColor
                           : kind == Kind::Splitter ? splitterColor
                           : kind == Kind::Laser    ? lightColors[emitter.colorIndex]
                                                    : ink;
            text(L"OBJECT INSPECTOR / " + std::to_wstring(number), 24, 22, 0, rgb(muted));
            text(label(), 24, 45, 2, rgb(accent));
            fill(24, 86, w - 48, 1, rgb(border));
            auto button = [&](int action, std::wstring name, float x, float y, float rw, bool chosen = false,
                              Color swatch = paper) {
                fill(x, y, rw, 32, rgb(chosen ? teal : Color{31, 46, 65, 255}));
                if (action >= 20 && action <= 26)
                    fill(x + 5, y + 5, rw - 10, 22, rgb(swatch));
                else
                    text(name, x + 10, y + 6, 1, rgb(ink));
                widgets.push_back({{px(x), px(y), px(x + rw), px(y + 32)}, action, false, 0, 0});
            };
            auto slider = [&](int action, const wchar_t* name, float y, float value, float lo, float hi,
                              const wchar_t* suffix) {
                text(name, 24, y, 1, rgb(ink));
                wchar_t valueText[64];
                std::swprintf(valueText, 64, L"%.0f%ls", value, suffix);
                text(valueText, w - 96, y, 1, rgb(accent));
                float fraction = std::clamp((value - lo) / (hi - lo), 0.f, 1.f);
                fill(29, y + 32, w - 58, 4, rgb(border));
                fill(29, y + 32, (w - 58) * fraction, 4, rgb(teal));
                fill(25 + (w - 58) * fraction, y + 27, 9, 14, rgb(accent));
                widgets.push_back({{px(29), px(y + 22), px(w - 29), px(y + 47)}, action, true, lo, hi});
            };
            if (kind == Kind::Prism) {
                text(L"MATERIAL", 24, 108, 0, rgb(muted));
                float step = (w - 48) / 3;
                button(4, L"Flint", 24, 132, step - 5, prism.glass == Glass::Flint);
                button(5, L"Crown", 24 + step, 132, step - 5, prism.glass == Glass::Crown);
                button(6, L"Water", 24 + step * 2, 132, step - 5, prism.glass == Glass::Water);
                slider(1, L"Apex angle", 190, prism.apex, 20, 100, L" deg");
                slider(2, L"Rotation", 264, prism.angle, -180, 180, L" deg");
                slider(3, L"Size", 338, prism.radius, 24, 210, L" px");
                button(7, reflections ? L"Glass reflections: on" : L"Glass reflections: off", 24, 412, w - 48,
                       reflections);
            } else if (kind == Kind::Splitter) {
                wchar_t valueText[64];
                std::swprintf(valueText, 64, L"Reflected %.0f%% / Transmitted %.0f%%", surface.reflectivity * 100,
                              (1 - surface.reflectivity) * 100);
                text(valueText, 24, 110, 1, rgb(splitterColor));
                slider(8, L"Reflected share", 154, surface.reflectivity * 100, 0, 100, L"%");
                float step = (w - 48) / 3;
                for (int i = 0; i < 3; ++i)
                    button(9 + i, std::to_wstring(25 + i * 25) + L"%", 24 + i * step, 216, step - 5);
                slider(12, L"Length", 284, surfaceLength(surface), 8, 1100, L" px");
                slider(13, L"Angle", 364,
                       std::atan2(surface.end.y - surface.start.y, surface.end.x - surface.start.x) * RAD2DEG, -180,
                       180, L" deg");
            } else if (kind == Kind::Laser) {
                button(30, L"Laser", 24, 110, (w - 58) / 2, emitter.mode == EmitterMode::Beam);
                button(31, L"Point", 29 + (w - 48) / 2, 110, (w - 58) / 2, emitter.mode == EmitterMode::Point);
                slider(14, L"Direction", 174, emitter.angle, -180, 180, L" deg");
                slider(15, L"Spread", 246, emitter.spread, 0, 120, L" deg");
                slider(16, L"Rays", 318, static_cast<float>(emitter.rayCount), 1, 240, L"");
                slider(17, L"Reflections", 390, static_cast<float>(emitter.bounces), 0, 12, L"");
                std::string colorLabel = std::string("COLOR / ") + colorNames[emitter.colorIndex];
                text(std::wstring(colorLabel.begin(), colorLabel.end()), 24, 462, 0, rgb(muted));
                float step = (w - 48) / 7;
                for (int i = 0; i < 7; ++i)
                    button(20 + i, L"", 24 + i * step, 486, step - 4, emitter.colorIndex == i, lightColors[i]);
                button(32, emitter.sweep ? L"Sweep: on" : L"Sweep: off", 24, 550, w - 48, emitter.sweep);
            } else {
                slider(12, L"Length", 120, surfaceLength(surface), 8, 1100, L" px");
                slider(13, L"Angle", 200,
                       std::atan2(surface.end.y - surface.start.y, surface.end.x - surface.start.x) * RAD2DEG, -180,
                       180, L" deg");
                text(L"Drag the object on the canvas to move it.", 24, 292, 1, rgb(muted));
            }
            text(L"Drag this window anywhere on the desktop.", 24, h - 30, 0, rgb(muted));
            return;
        }
        text(L"LIVE DETECTOR / D" + std::to_wstring(number), 24, 22, 0, rgb(muted));
        text(L"Light detector", 24, 45, 2, rgb(detectorColor));
        fill(24, 86, w - 48, 1, rgb(border));
        text(L"RECEIVED POWER", 24, 106, 0, rgb(muted));
        wchar_t formatted[96];
        std::swprintf(formatted, 96, L"%.1f%%", reading.power * 100);
        text(formatted, 24, 128, 3, rgb(detectorColor));
        text(L"of total laser power", 24, 183, 1, rgb(muted));
        float minimum = 1, maximum = 0;
        for (auto band : reading.spectrum) {
            minimum = std::min(minimum, band.weight);
            maximum = std::max(maximum, band.weight);
        }
        std::wstring description = L"No light received";
        if (reading.power > 0) {
            if (reading.spectrum.size() > 1 && maximum - minimum < maximum * .03f) {
                description = std::to_wstring(reading.spectrum.size()) + L" bands / balanced power";
            } else {
                std::swprintf(formatted, 96, L"Peak band: %.0f nm", reading.peakWavelength);
                description = formatted;
            }
        }
        text(description, 24, 222, 1, rgb(ink));
        text(L"SPECTRUM / relative height", 24, 258, 0, rgb(muted));
        const float chartY = 284, chartH = std::max(90.f, h - 360);
        const float chartW = w - 48;
        fill(24, chartY, chartW, chartH, RGB(8, 16, 27));
        chart = {px(24), px(chartY), px(w - 24), px(chartY + chartH)};
        for (auto band : reading.spectrum) {
            float x = 24 + (band.wavelength - 400) / 320 * chartW;
            float barH = maximum > 0 ? band.weight / maximum * (chartH - 10) : 0;
            fill(x - 4, chartY + chartH - barH, 8, barH, rgb(wavelengthColor(band.wavelength)));
        }
        fill(24, chartY + chartH, chartW, 1, rgb(border));
        text(L"400", 24, chartY + chartH + 8, 0, rgb(muted));
        text(L"720 nm", w - 70, chartY + chartH + 8, 0, rgb(muted));
        if (hoverBand >= 0 && hoverBand < static_cast<int>(reading.spectrum.size())) {
            auto band = reading.spectrum[hoverBand];
            std::swprintf(formatted, 96, L"%.0f nm / %.2f%% of source", band.wavelength, band.weight * 100);
            text(formatted, 24, h - 30, 0, rgb(detectorColor));
        } else
            text(L"Drag the title bar to move this window.", 24, h - 30, 0, rgb(muted));
    }
};

LRESULT CALLBACK meterProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* meter = reinterpret_cast<Meter*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        meter = static_cast<Meter*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        meter->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(meter));
    }
    if (!meter)
        return DefWindowProcW(window, message, wParam, lParam);
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        RECT client;
        GetClientRect(window, &client);
        int width = client.right, height = client.bottom;
        if (width > 0 && height > 0) {
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
            auto old = SelectObject(memory, bitmap);
            meter->draw(memory, width, height);
            BitBlt(dc, 0, 0, width, height, memory, 0, 0, SRCCOPY);
            SelectObject(memory, old);
            DeleteObject(bitmap);
            DeleteDC(memory);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_GETMINMAXINFO: {
        UINT dpi = GetDpiForWindow(window);
        auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize = {MulDiv(340, dpi ? dpi : 96, 96), MulDiv(meter->kind == Kind::Detector ? 500
                                                                          : meter->kind == Kind::Laser  ? 700
                                                                                                        : 560,
                                                                          dpi ? dpi : 96, 96)};
        return 0;
    }
    case WM_DPICHANGED: {
        auto* rect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT point{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
        for (auto widget : meter->widgets)
            if (PtInRect(&widget.rect, point)) {
                if (meter->app)
                    meter->app->checkpoint();
                if (widget.slider) {
                    meter->activeAction = widget.action;
                    SetCapture(window);
                    meter->adjust(point);
                } else
                    meter->apply(widget.action, 0);
                break;
            }
        return 0;
    }
    case WM_LBUTTONUP:
        meter->activeAction = 0;
        if (GetCapture() == window)
            ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        meter->activeAction = 0;
        return 0;
    case WM_MOUSEMOVE: {
        POINT point{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
        if (meter->activeAction) {
            meter->adjust(point);
            return 0;
        }
        meter->hoverBand = -1;
        if (meter->kind == Kind::Detector && PtInRect(&meter->chart, point)) {
            float wavelength =
                400 + (point.x - meter->chart.left) * 320.f / std::max(1L, meter->chart.right - meter->chart.left);
            float closest = 16;
            for (size_t i = 0; i < meter->reading.spectrum.size(); ++i) {
                float delta = std::abs(meter->reading.spectrum[i].wavelength - wavelength);
                if (delta < closest) {
                    closest = delta;
                    meter->hoverBand = static_cast<int>(i);
                }
            }
        }
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSELEAVE:
        meter->hoverBand = -1;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_WINDOWPOSCHANGED:
        GetWindowRect(window, &meter->lastBounds);
        break;
    case WM_CLOSE:
        GetWindowRect(window, &meter->lastBounds);
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        meter->window = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    }
    // No WM_QUIT is posted when a detector is closed; the simulation keeps running.
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace

struct DetectorWindows::Impl {
    HWND owner;
    bool hidden;
    std::vector<std::unique_ptr<Meter>> meters;
    std::map<std::uint64_t, RECT> positions;

    Impl(void* owner, bool hidden) : owner(static_cast<HWND>(owner)), hidden(hidden) {
        WNDCLASSEXW cls{};
        cls.cbSize = sizeof(cls);
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = meterProcedure;
        cls.lpszClassName = meterClass;
        cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassExW(&cls); // Registration can already exist for another instance.
    }

    Meter* find(std::uint64_t id) const {
        for (const auto& meter : meters)
            if (meter->id == id && meter->window)
                return meter.get();
        return nullptr;
    }

    void open(App& app, std::uint64_t id) {
        if (auto* existing = find(id)) {
            if (!hidden) {
                ShowWindow(existing->window, SW_SHOWNOACTIVATE);
                SetWindowPos(existing->window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return;
        }
        auto meter = std::make_unique<Meter>();
        meter->id = id;
        if (!meter->sync(app))
            return;
        UINT dpi = GetDpiForWindow(owner);
        if (!dpi)
            dpi = 96;
        int width = MulDiv(390, dpi, 96), height = MulDiv(meter->kind == Kind::Laser ? 700 : 560, dpi, 96);
        RECT ownerRect{};
        GetWindowRect(owner, &ownerRect);
        MONITORINFO monitor{sizeof(monitor), {}, {}, 0};
        GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
        int x = std::clamp(
            static_cast<int>(ownerRect.right) + 16, static_cast<int>(monitor.rcWork.left),
            std::max(static_cast<int>(monitor.rcWork.left), static_cast<int>(monitor.rcWork.right) - width - 12));
        int y = std::clamp(
            static_cast<int>(ownerRect.top) + 70 + static_cast<int>(meters.size()) * 28,
            static_cast<int>(monitor.rcWork.top),
            std::max(static_cast<int>(monitor.rcWork.top), static_cast<int>(monitor.rcWork.bottom) - height));
        auto previous = positions.find(id);
        if (previous != positions.end() && MonitorFromRect(&previous->second, MONITOR_DEFAULTTONULL)) {
            x = previous->second.left;
            y = previous->second.top;
            width = previous->second.right - x;
            height = previous->second.bottom - y;
        }
        std::wstring title = meter->title();
        HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, meterClass, title.c_str(), WS_OVERLAPPEDWINDOW, x, y, width,
                                      height, owner, nullptr, GetModuleHandleW(nullptr), meter.get());
        if (!window) {
            TraceLog(LOG_WARNING, "Detector window creation failed: %lu", GetLastError());
            return;
        }
        BOOL dark = TRUE;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        if (!hidden)
            ShowWindow(window, SW_SHOWNOACTIVATE);
        meters.push_back(std::move(meter));
    }
};

DetectorWindows::DetectorWindows(void* owner, bool hidden) : impl(std::make_unique<Impl>(owner, hidden)) {}

DetectorWindows::~DetectorWindows() {
    closeAll();
}

void DetectorWindows::update(App& app) {
    app.ensureSurfaceIds();
    for (auto& meter : impl->meters) {
        if (meter->window)
            GetWindowRect(meter->window, &impl->positions[meter->id]);
        else
            impl->positions[meter->id] = meter->lastBounds;
    }
    impl->meters.erase(
        std::remove_if(impl->meters.begin(), impl->meters.end(), [](const auto& meter) { return !meter->window; }),
        impl->meters.end());
    for (auto id : app.detectorWindowRequests)
        impl->open(app, id);
    app.detectorWindowRequests.clear();
    for (auto& meter : impl->meters) {
        if (!meter->sync(app)) {
            if (meter->window)
                DestroyWindow(meter->window);
            continue;
        }
        auto title = meter->title();
        SetWindowTextW(meter->window, title.c_str());
        InvalidateRect(meter->window, nullptr, FALSE);
        if (!impl->hidden && !IsIconic(meter->window))
            UpdateWindow(meter->window);
    }
    // raylib/GLFW dispatches the thread's native messages during its frame loop.
}

void DetectorWindows::closeAll() {
    for (auto& meter : impl->meters)
        if (meter->window)
            DestroyWindow(meter->window);
    impl->meters.clear();
}

std::size_t DetectorWindows::count() const {
    return std::count_if(impl->meters.begin(), impl->meters.end(),
                         [](const auto& meter) { return meter->window != nullptr; });
}

void* DetectorWindows::nativeHandle(std::uint64_t id) const {
    auto* meter = impl->find(id);
    return meter ? meter->window : nullptr;
}

float DetectorWindows::displayedPower(std::uint64_t id) const {
    auto* meter = impl->find(id);
    return meter ? meter->reading.power : 0;
}

bool DetectorWindows::captureFirst(const std::string& path) const {
    return captureWindow(0, path);
}

bool DetectorWindows::captureWindow(std::uint64_t id, const std::string& path) const {
    for (const auto& meter : impl->meters) {
        if (!meter->window || (id && meter->id != id))
            continue;
        RECT client{};
        GetClientRect(meter->window, &client);
        int width = client.right, height = client.bottom;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap)
            return false;
        HDC memory = CreateCompatibleDC(nullptr);
        auto old = SelectObject(memory, bitmap);
        meter->draw(memory, width, height);
        GdiFlush();
        BITMAPFILEHEADER header{};
        header.bfType = 0x4d42;
        header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
        header.bfSize = header.bfOffBits + width * height * 4;
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
        file.write(static_cast<const char*>(pixels), width * height * 4);
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
        return static_cast<bool>(file);
    }
    return false;
}
} // namespace lightlab
