#include "native_windows.hpp"
#include "detector_window_checks.hpp"
#include "detector_windows.hpp"
#include "app.hpp"
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <chrono>

namespace lightlab {
int runDetectorWindowChecks(App& app, DetectorWindows& windows, void* ownerHandle) {
    std::ofstream report("detector-window-checks.txt");
    auto check = [&](bool condition, const char* label) {
        report << (condition ? "PASS " : "FAIL ") << label << '\n';
        if (!condition)
            throw std::runtime_error(label);
    };
    auto refresh = [&] {
        app.ensureSurfaceIds();
        app.rays = traceScene(app.scene);
        app.readings = readDetectors(app.scene, app.rays);
        windows.update(app);
    };
    auto owner = static_cast<HWND>(ownerHandle);
    try {
        windows.closeAll();
        app.choosePreset(Preset::SplitterLab);
        app.requestDetectorWindow(1);
        app.requestDetectorWindow(2);
        refresh();
        const auto leftId = app.scene.surfaces[1].id, rightId = app.scene.surfaces[2].id;
        auto first = static_cast<HWND>(windows.nativeHandle(leftId));
        auto second = static_cast<HWND>(windows.nativeHandle(rightId));
        check(windows.count() == 2 && first != second && IsWindow(first) && IsWindow(second),
              "two detectors have independent native windows");
        check(!(GetWindowLongPtrW(first, GWL_STYLE) & WS_CHILD) && GetWindow(first, GW_OWNER) == owner,
              "detector inspector is a desktop top-level window, not a canvas child");
        check((GetWindowLongPtrW(first, GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME)) == (WS_CAPTION | WS_THICKFRAME),
              "detector has the native draggable title bar and resize border");
        RECT mainRect{}, moved{};
        GetWindowRect(owner, &mainRect);
        SetWindowPos(first, nullptr, mainRect.right + 200, mainRect.top + 40, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        GetWindowRect(first, &moved);
        check(moved.left == mainRect.right + 200, "detector window moves entirely outside application bounds");
        SetWindowPos(second, nullptr, -700, -150, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        GetWindowRect(second, &moved);
        check(moved.left == -700 && moved.top == -150,
              "negative desktop coordinates are supported for other monitor layouts");
        SetWindowPos(first, nullptr, 0, 0, 480, 640, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
        GetWindowRect(first, &moved);
        check(moved.right - moved.left == 480 && moved.bottom - moved.top == 640,
              "detector window resizes independently");
        app.selected = -1;
        app.scene.surfaces[0].reflectivity = .75f;
        refresh();
        check(windows.count() == 2 && std::abs(windows.displayedPower(leftId) - .75f) < .001f &&
                  std::abs(windows.displayedPower(rightId) - .25f) < .001f,
              "detached readings update live when another object is edited");
        app.requestDetectorWindow(1);
        refresh();
        check(windows.count() == 2 && windows.nativeHandle(leftId) == first,
              "clicking the same detector reuses its window");
        check(windows.captureFirst("detector-native-preview.bmp"), "native spectrum panel renders to a capture");
        MONITORINFO monitor{sizeof(monitor), {}, {}, 0};
        GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
        SetWindowPos(first, nullptr, monitor.rcWork.left + 50, monitor.rcWork.top + 50, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
        GetWindowRect(first, &moved);
        SendMessageW(first, WM_CLOSE, 0, 0);
        refresh();
        check(windows.count() == 1 && IsWindow(owner) && !WindowShouldClose() && app.scene.surfaces.size() == 3,
              "closing a detector window keeps the simulation and detector intact");
        app.requestDetectorWindow(1);
        refresh();
        first = static_cast<HWND>(windows.nativeHandle(leftId));
        RECT reopened{};
        GetWindowRect(first, &reopened);
        check(IsWindow(first) && windows.count() == 2 && reopened.left == moved.left && reopened.top == moved.top,
              "clicking a closed detector reopens it at its remembered position");
        app.selected = 1;
        app.eraseSelection();
        refresh();
        check(windows.count() == 1 && !windows.nativeHandle(leftId) && windows.nativeHandle(rightId) == second &&
                  std::abs(windows.displayedPower(rightId) - .25f) < .001f,
              "deleting a detector closes only its window and preserves other detector bindings");
        app.undo();
        refresh();
        app.requestDetectorWindow(1);
        refresh();
        check(windows.count() == 2 && windows.nativeHandle(leftId) && windows.nativeHandle(rightId) == second,
              "undo restores detector identity without reassigning other windows");
        app.choosePreset(Preset::Blank);
        refresh();
        check(windows.count() == 0 && IsWindow(owner), "replacing the scene cleans up obsolete detector windows");
        app.choosePreset(Preset::SplitterLab);
        app.scene.prisms.push_back({{450, 260}, 100, 57});
        app.scene.surfaces.push_back({{150, 100}, {250, 100}, Material::Mirror});
        app.scene.surfaces.push_back({{150, 150}, {250, 150}, Material::Absorber});
        app.ensureSurfaceIds();
        auto prismId = app.scene.prisms[0].id, laserId = app.scene.lasers[0].id;
        auto splitterId = app.scene.surfaces[0].id, mirrorId = app.scene.surfaces[3].id,
             wallId = app.scene.surfaces[4].id;
        app.detectorWindowRequests = {prismId, laserId, splitterId, mirrorId, wallId, app.scene.surfaces[1].id};
        refresh();
        check(windows.count() == 6, "all six object types have independent desktop inspectors");
        for (auto id : {prismId, laserId, splitterId, mirrorId, wallId}) {
            auto handle = static_cast<HWND>(windows.nativeHandle(id));
            check(handle && !(GetWindowLongPtrW(handle, GWL_STYLE) & WS_CHILD),
                  "object inspector is not confined to the application");
            check(windows.captureWindow(id, "object-" + std::to_string(id) + ".bmp"),
                  "object inspector renders controls");
        }
        auto click = [&](std::uint64_t id, float x, float y) {
            auto handle = static_cast<HWND>(windows.nativeHandle(id));
            float scale = GetDpiForWindow(handle) / 96.f;
            auto position =
                MAKELPARAM(static_cast<int>(std::round(x * scale)), static_cast<int>(std::round(y * scale)));
            SendMessageW(handle, WM_LBUTTONDOWN, MK_LBUTTON, position);
            SendMessageW(handle, WM_LBUTTONUP, 0, position);
        };
        click(prismId, 170, 148);
        check(app.scene.prisms[0].glass == Glass::Crown, "native prism material buttons update the bound prism");
        click(prismId, 270, 224);
        check(app.scene.prisms[0].apex > 80, "native prism apex slider changes geometry");
        click(prismId, 100, 298);
        check(app.scene.prisms[0].angle < -90, "native prism rotation slider works");
        click(prismId, 300, 372);
        check(app.scene.prisms[0].radius > 180, "native prism size slider works");
        click(prismId, 100, 428);
        check(!app.scene.partialReflections, "native prism reflection toggle works");
        click(splitterId, 280, 230);
        refresh();
        check(std::abs(app.scene.surfaces[0].reflectivity - .75f) < .001f,
              "native splitter presets update reflectivity");
        click(splitterId, 100, 188);
        check(app.scene.surfaces[0].reflectivity > .2f && app.scene.surfaces[0].reflectivity < .3f,
              "native splitter slider edits the ratio");
        click(mirrorId, 230, 154);
        check(surfaceLength(app.scene.surfaces[3]) > 600, "native mirror length control works");
        click(wallId, 250, 234);
        check(app.scene.surfaces[4].end.y > app.scene.surfaces[4].start.y + 50, "native wall rotation control works");
        click(laserId, 215, 126);
        check(app.scene.lasers[0].mode == EmitterMode::Point, "native source mode control works");
        click(laserId, 250, 208);
        check(app.scene.lasers[0].angle > 60, "native source direction slider works");
        click(laserId, 230, 280);
        check(app.scene.lasers[0].spread > 70, "native source spread slider works");
        click(laserId, 260, 352);
        check(app.scene.lasers[0].rayCount > 170, "native source ray count control works");
        click(laserId, 90, 502);
        click(laserId, 275, 502);
        check(app.scene.lasers[0].colorIndex == 5, "native source color buttons work");
        app.undo();
        check(app.scene.lasers[0].colorIndex != 5, "native inspector edits participate in Undo");
        app.scene.prisms[0] = {{450, 260}, 100, 57, 60, Glass::Flint, prismId};
        refresh();
        check(windows.captureWindow(prismId, "native-prism-controls.bmp") &&
                  windows.captureWindow(splitterId, "native-splitter-controls.bmp") &&
                  windows.captureWindow(laserId, "native-laser-controls.bmp"),
              "final native object controls render correctly");
        Emitter secondLaser;
        secondLaser.position = {300, 500};
        secondLaser.colorIndex = 2;
        secondLaser.rayCount = 1;
        app.scene.lasers.push_back(secondLaser);
        app.selected = -2;
        app.selectedLaser = 1;
        app.requestSelectionWindow();
        refresh();
        const auto secondId = app.scene.lasers[1].id;
        auto secondHandle = windows.nativeHandle(secondId);
        check(secondHandle && secondHandle != windows.nativeHandle(laserId) && windows.count() == 7 &&
                  windows.captureWindow(secondId, "native-second-laser.bmp"),
              "multiple lasers have independent native settings windows");
        click(secondId, 275, 502);
        check(app.scene.lasers[1].colorIndex == 5 && app.scene.lasers[0].colorIndex == 1,
              "changing one laser color leaves the other laser unchanged");
        click(secondId, 300, 424);
        check(app.scene.lasers[1].bounces >= 10 && app.scene.lasers[0].bounces == 6,
              "reflection controls are per laser");
        click(secondId, 100, 566);
        float firstAngle = app.scene.lasers[0].angle, secondAngle = app.scene.lasers[1].angle;
        app.update(.25f);
        check(app.scene.lasers[1].sweep && !app.scene.lasers[0].sweep && app.scene.lasers[0].angle == firstAngle &&
                  app.scene.lasers[1].angle > secondAngle,
              "sweep animation affects only the configured laser");
        app.selected = -2;
        app.selectedLaser = 0;
        app.eraseSelection();
        refresh();
        check(app.scene.lasers.size() == 1 && app.scene.lasers[0].id == secondId &&
                  windows.nativeHandle(secondId) == secondHandle && !windows.nativeHandle(laserId),
              "deleting a laser preserves other laser window bindings");
        app.undo();
        refresh();
        check(app.scene.lasers.size() == 2 && app.scene.lasers[0].id == laserId && app.scene.lasers[1].id == secondId,
              "Undo restores deleted laser identity and independent settings");
        windows.closeAll();
        auto sceneRoot =
            std::filesystem::temp_directory_path() /
            ("lightlab-app-scenes-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        {
            App harness(true);
            harness.library = std::make_unique<SceneLibrary>(sceneRoot);
            harness.library->restore(harness.scene);
            harness.scene.lasers[0].angle = 25;
            harness.choosePreset(Preset::Blank);
            auto saved = harness.library->savedScenes();
            check(saved.size() == 1, "clicking a new starter auto-saves unnamed edited work");
            auto blankPath = harness.library->scenesDirectory() / "Blank experiment.lightscene";
            check(harness.saveNamed(blankPath.u8string()), "application Save As creates a named scene");
            harness.scene.lasers[0].angle = 42;
            harness.choosePreset(Preset::Relay);
            harness.undo();
            check(harness.scene.preset == Preset::Blank && harness.scene.lasers[0].angle == 42 &&
                      harness.library->currentPath() == blankPath,
                  "undoing a scene switch restores the correct scene and save destination");
            check(harness.openNamed(saved[0].u8string()) && harness.scene.lasers[0].angle == 25,
                  "application Open restores independently saved work");
            harness.undo();
            check(harness.scene.lasers[0].angle == 42 && harness.library->currentPath() == blankPath,
                  "undoing Open restores the previous named scene");
        }
        std::error_code cleanup;
        std::filesystem::remove_all(sceneRoot, cleanup);
        return 0;
    } catch (const std::exception& error) {
        report << "FAIL native window checks: " << error.what() << '\n';
        windows.closeAll();
        return 2;
    }
}
} // namespace lightlab
