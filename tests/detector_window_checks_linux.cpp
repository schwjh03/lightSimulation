#ifndef _WIN32
#include "detector_window_checks.hpp"
#include "detector_windows.hpp"
#include "app.hpp"
#define Font X11Font
#define Window X11Window
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#undef Window
#undef Font
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace lightlab {
int runDetectorWindowChecks(App& app, DetectorWindows& windows, void*) {
    std::ofstream report("detector-window-checks.txt");
    auto check = [&](bool condition, const char* label) {
        report << (condition ? "PASS " : "FAIL ") << label << '\n';
        if (!condition)
            throw std::runtime_error(label);
    };
    auto refresh = [&] {
        app.rays = traceScene(app.scene);
        app.readings = readDetectors(app.scene, app.rays);
        windows.update(app);
    };
    try {
        windows.closeAll();
        app.choosePreset(Preset::SplitterLab);
        app.requestDetectorWindow(1);
        app.requestDetectorWindow(2);
        refresh();
        auto first = app.scene.surfaces[1].id;
        auto second = app.scene.surfaces[2].id;
        check(windows.count() == 2 && windows.nativeHandle(first) && windows.nativeHandle(second) &&
                  windows.nativeHandle(first) != windows.nativeHandle(second),
              "independent X11 detector windows");
        auto secondHandle = windows.nativeHandle(second);
        check(windows.captureFirst("detector-linux-preview.bmp"), "inspector capture works for hidden windows");
        check(std::abs(windows.displayedPower(first) - .5f) < .001f &&
                  std::abs(windows.displayedPower(second) - .5f) < .001f,
              "live detector power");
        app.scene.surfaces[0].reflectivity = .75f;
        refresh();
        check(std::abs(windows.displayedPower(first) - .75f) < .001f &&
                  std::abs(windows.displayedPower(second) - .25f) < .001f,
              "splitter edits update readings");
        app.checkpoint();
        app.scene.surfaces.erase(app.scene.surfaces.begin() + 1);
        refresh();
        check(windows.count() == 1 && !windows.nativeHandle(first) && windows.nativeHandle(second) == secondHandle,
              "deletion closes only the matching inspector");
        app.undo();
        app.requestDetectorWindow(1);
        refresh();
        check(windows.count() == 2 && windows.nativeHandle(first), "undo allows reopening an inspector");
        app.selected = -2;
        app.selectedLaser = 0;
        app.requestSelectionWindow();
        refresh();
        check(windows.count() == 3 && windows.nativeHandle(app.scene.lasers[0].id),
              "laser inspector opens independently");
        Display* input = XOpenDisplay(nullptr);
        check(input != nullptr, "open inspector input connection");
        auto laserWindow = reinterpret_cast<std::uintptr_t>(windows.nativeHandle(app.scene.lasers[0].id));
        auto send = [&](int type, int x, int y) {
            XEvent event{};
            event.xany.type = type;
            event.xany.display = input;
            event.xany.window = laserWindow;
            long mask;
            if (type == MotionNotify) {
                event.xmotion.x = x;
                event.xmotion.y = y;
                event.xmotion.state = Button1Mask;
                mask = ButtonMotionMask;
            } else {
                event.xbutton.x = x;
                event.xbutton.y = y;
                event.xbutton.button = Button1;
                mask = type == ButtonPress ? ButtonPressMask : ButtonReleaseMask;
            }
            XSendEvent(input, laserWindow, False, mask, &event);
            XSync(input, False);
            refresh();
        };
        float originalAngle = app.scene.lasers[0].angle;
        send(ButtonPress, 190, 80);
        check(std::abs(app.scene.lasers[0].angle) < .001f, "slider click sets value");
        send(MotionNotify, 275, 400);
        check(std::abs(app.scene.lasers[0].angle - 90) < .001f, "slider drags outside its row");
        send(MotionNotify, 450, 400);
        check(std::abs(app.scene.lasers[0].angle - 180) < .001f, "slider drag clamps at maximum");
        send(ButtonRelease, -20, 400);
        check(std::abs(app.scene.lasers[0].angle + 180) < .001f, "slider release clamps at minimum");
        send(MotionNotify, 190, 80);
        check(std::abs(app.scene.lasers[0].angle + 180) < .001f, "released slider stops dragging");
        app.undo();
        check(std::abs(app.scene.lasers[0].angle - originalAngle) < .001f, "one undo restores entire drag");
        send(ButtonPress, 80, 368);
        send(ButtonRelease, 80, 368);
        check(app.scene.lasers[0].mode == EmitterMode::Wall && app.scene.lasers[0].rayCount == 9,
              "laser inspector can switch to parallel wall mode");
        send(ButtonPress, 190, 125);
        send(ButtonRelease, 190, 125);
        check(std::abs(toCentimeters(app.scene.lasers[0].width) - 20.4f) < .001f,
              "laser wall width slider uses centimeters");
        app.undo();
        app.undo();
        refresh();
        check(app.scene.lasers[0].mode == EmitterMode::Beam, "undo restores laser mode and width changes");
        Prism lens;
        lens.lens = LensShape::PlanoConvex;
        app.scene.prisms.push_back(lens);
        app.selected = -3;
        app.selectedPrism = 0;
        app.requestSelectionWindow();
        refresh();
        laserWindow = reinterpret_cast<std::uintptr_t>(windows.nativeHandle(app.scene.prisms[0].id));
        check(laserWindow != 0, "lens inspector opens independently");
        float originalThickness = lensThickness(app.scene.prisms[0]);
        send(ButtonPress, 190, 156);
        send(ButtonRelease, 190, 156);
        check(std::abs(app.scene.prisms[0].radius - 117) < .001f &&
                  std::abs(lensThickness(app.scene.prisms[0]) - originalThickness) < .001f,
              "lens Length slider preserves center thickness");
        send(ButtonPress, 190, 208);
        send(ButtonRelease, 190, 208);
        check(lensThickness(app.scene.prisms[0]) > originalThickness &&
                  std::abs(app.scene.prisms[0].radius - 117) < .001f,
              "lens Thickness slider preserves length");
        app.undo();
        app.undo();
        refresh();
        send(ButtonPress, 80, 492);
        send(ButtonRelease, 80, 492);
        check(app.scene.prisms[0].flipped, "lens inspector Flip reverses orientation");
        app.undo();
        check(!app.scene.prisms[0].flipped, "lens flip can be undone");
        // Lens rows: heading, rotation, length, thickness, material buttons, n slider, Fresnel, Flip.
        send(ButtonPress, 190, 402);
        send(ButtonRelease, 190, 402);
        check(std::abs(objectIndex(589, app.scene.prisms[0]) - 1.75f) < .001f, "n slider changes optical index");
        for (int i = 0; i < 3; ++i) {
            send(ButtonPress, 80, 268 + i * 38);
            send(ButtonRelease, 80, 268 + i * 38);
            check(app.scene.prisms[0].customIndex == 0 &&
                      std::abs(objectIndex(589, app.scene.prisms[0]) - glassIndex(589, static_cast<Glass>(i))) < .0001f,
                  "material preset resets n slider to its index");
        }
        // Exercise actual on-screen presentation, not the offscreen capture helper.
        // Keep the test window independent of asynchronous window-manager placement.
        XSetWindowAttributes testAttributes{};
        testAttributes.override_redirect = True;
        XChangeWindowAttributes(input, laserWindow, CWOverrideRedirect, &testAttributes);
        XMapRaised(input, laserWindow);
        XSync(input, False);
        refresh();
        auto completeHeader = [&](int width) {
            // The renderer and observer use separate X connections. Another refresh
            // waits for the renderer's prior frame before the observer samples it.
            refresh();
            XSync(input, False);
            XImage* pixels = XGetImage(input, laserWindow, width - 85, 15, 1, 1, AllPlanes, ZPixmap);
            if (!pixels)
                return false;
            unsigned long pixel = XGetPixel(pixels, 0, 0), mask = pixels->red_mask;
            while (mask && !(mask & 1)) {
                mask >>= 1;
                pixel >>= 1;
            }
            bool complete = mask && (pixel & mask) * 255 / mask == 163;
            if (!complete)
                report << "Header pixel red=" << (mask ? (pixel & mask) * 255 / mask : 0) << " width=" << width << '\n';
            XDestroyImage(pixels);
            return complete;
        };
        bool stable = true;
        auto handleBefore = windows.nativeHandle(app.scene.prisms[0].id);
        send(ButtonPress, 190, 402);
        for (int i = 0; i < 32; ++i) {
            send(MotionNotify, 20 + i * 10, 402);
            stable = stable && completeHeader(390) && windows.nativeHandle(app.scene.prisms[0].id) == handleBefore;
        }
        send(ButtonRelease, 330, 402);
        for (int i = 0; i < 12; ++i) {
            send(ButtonPress, 80, 462);
            send(ButtonRelease, 80, 462);
            send(ButtonPress, 80, 492);
            send(ButtonRelease, 80, 492);
            stable = stable && completeHeader(390);
        }
        check(stable, "popup presents complete frames during repeated slider drags and button changes");
        XResizeWindow(input, laserWindow, 460, 760);
        XSync(input, False);
        refresh();
        check(completeHeader(460), "resized popup redraws a complete frame at the new dimensions");
        XUnmapWindow(input, laserWindow);
        XSync(input, False);
        for (int type = 0; type < 7; ++type) {
            windows.closeAll();
            app.choosePreset(Preset::Blank);
            if (type >= 1 && type <= 4) {
                app.scene.surfaces.push_back({{600, 100}, {600, 300}, static_cast<Material>(type - 1)});
                app.selected = 0;
            } else if (type >= 5) {
                Prism object;
                if (type == 6)
                    object.lens = LensShape::Biconvex;
                app.scene.prisms.push_back(object);
                app.selected = -3;
                app.selectedPrism = 0;
            } else {
                app.selected = -2;
                app.selectedLaser = 0;
            }
            app.requestSelectionWindow();
            refresh();
            std::uint64_t target = type == 0   ? app.scene.lasers[0].id
                                   : type <= 4 ? app.scene.surfaces[0].id
                                               : app.scene.prisms[0].id;
            app.scene.lasers.push_back(Emitter{});
            app.selected = -2;
            app.selectedLaser = 1;
            app.requestSelectionWindow();
            refresh();
            auto other = app.scene.lasers[1].id;
            auto before = sceneData(app.scene);
            laserWindow = reinterpret_cast<std::uintptr_t>(windows.nativeHandle(target));
            send(ButtonPress, 330, 24);
            check(!windows.nativeHandle(target) && windows.nativeHandle(other) && windows.count() == 1,
                  "inspector Delete removes its own object and closes only its window");
            check(app.selected == -1, "inspector deletion clears stale selection");
            app.undo();
            check(sceneData(app.scene) == before, "one undo restores object deleted from inspector");
        }
        XCloseDisplay(input);
        app.choosePreset(Preset::Blank);
        refresh();
        check(windows.count() == 0, "scene replacement closes obsolete inspectors");
        windows.closeAll();
        return 0;
    } catch (const std::exception& error) {
        report << "FAIL " << error.what() << '\n';
        windows.closeAll();
        return 2;
    }
}
}
#endif
