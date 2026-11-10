#ifndef _WIN32
#include "detector_window_checks.hpp"
#include "detector_windows.hpp"
#include "app.hpp"
#define Font X11Font
#define Window X11Window
#include <X11/Xlib.h>
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
