#include "app.hpp"
#include "detector_windows.hpp"
#include "scene_dialogs.hpp"
#include "../tests/ui_checks.hpp"
#include "../tests/detector_window_checks.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace lightlab {
int runWindow(int argc, char** argv) {
    bool testing = false, smoke = false, small = false;
    std::string capture;
    Preset preset = Preset::Room;
    bool point = false, multi = false;
    int colorOverride = -1;
    int inspect = 0;
    float backgroundStart = 0;
    for (int i = 1; i < argc; ++i) {
        std::string argument = argv[i];
        if (argument == "--test-session") {
            testing = true;
        } else if (argument == "--smoke-test") {
            testing = true;
            smoke = true;
        } else if (argument == "--capture" && i + 1 < argc) {
            testing = true;
            capture = argv[++i];
        } else if (argument == "--small") {
            small = true;
        } else if (argument == "--relay") {
            testing = true;
            preset = Preset::Relay;
        } else if (argument == "--point") {
            testing = true;
            point = true;
        } else if (argument == "--prism") {
            testing = true;
            preset = Preset::Dispersion;
        } else if (argument == "--splitter") {
            testing = true;
            preset = Preset::SplitterLab;
        } else if (argument == "--multi") {
            testing = true;
            multi = true;
        } else if (argument == "--inspect-prism") {
            testing = true;
            inspect = 1;
        } else if (argument == "--inspect-detector") {
            testing = true;
            inspect = 2;
        } else if (argument == "--inspect-splitter") {
            testing = true;
            inspect = 3;
        } else if (argument == "--inspect-laser") {
            testing = true;
            inspect = 4;
        } else if (argument == "--background-time" && i + 1 < argc) {
            testing = true;
            try {
                backgroundStart = std::clamp(std::stof(argv[++i]), 0.f, 10000.f);
            } catch (...) {
                return 1;
            }
            if (!std::isfinite(backgroundStart))
                return 1;
        } else if (argument == "--color" && i + 1 < argc) {
            testing = true;
            try {
                colorOverride = std::clamp(std::stoi(argv[++i]), 0, 6);
            } catch (...) {
                return 1;
            }
        }
    }
    bool hidden = smoke || !capture.empty();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | (hidden ? FLAG_WINDOW_HIDDEN : 0));
    InitWindow(small ? 960 : 1280, small ? 600 : 800, "Light Lab 2D - Lasers, Prisms and Spectra");
    if (!IsWindowReady()) {
        return 1;
    }
    SetWindowMinSize(960, 600);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
    Image icon = GenImageColor(64, 64, paper);
    ImageDrawCircle(&icon, 23, 32, 8, teal);
    ImageDrawLine(&icon, 30, 32, 57, 12, teal);
    ImageDrawLine(&icon, 30, 32, 59, 32, teal);
    ImageDrawLine(&icon, 30, 32, 57, 52, teal);
    SetWindowIcon(icon);
    UnloadImage(icon);
    RenderTexture2D canvasTexture = LoadRenderTexture(1280, 800);
    SetTextureFilter(canvasTexture.texture, TEXTURE_FILTER_BILINEAR);
    int result = 0;
    {
        App app(testing);
        if (testing) {
            app.scene = makeScene(preset);
        }
        if (multi) {
            app.scene = makeScene(Preset::Blank);
            app.scene.lasers.clear();
            const int colors[] = {1, 2, 6};
            for (int i = 0; i < 3; ++i) {
                Emitter light;
                light.position = {150, 180.f + i * 170};
                light.angle = 0;
                light.colorIndex = colors[i];
                light.rayCount = 1;
                app.scene.lasers.push_back(light);
            }
            app.scene.surfaces.push_back({{850, 100}, {850, 620}, Material::Detector});
        }
        if (point) {
            app.scene.lasers[0].mode = EmitterMode::Point;
        }
        if (colorOverride >= 0)
            app.scene.lasers[0].colorIndex = colorOverride;
        app.backgroundTime = backgroundStart;
        if (inspect == 1 && !app.scene.prisms.empty()) {
            app.selected = -3;
            app.selectedPrism = 0;
        }
        if (inspect == 2 || inspect == 3) {
            for (size_t i = 0; i < app.scene.surfaces.size(); ++i) {
                auto kind = inspect == 2 ? Material::Detector : Material::Splitter;
                if (app.scene.surfaces[i].material == kind) {
                    app.selected = static_cast<int>(i);
                    break;
                }
            }
        }
        if (inspect == 4 && !app.scene.lasers.empty()) {
            app.selected = -2;
            app.selectedLaser = 0;
        }
        app.ui.load();
        DetectorWindows detectorWindows(GetWindowHandle(), hidden);
        if (inspect)
            app.requestSelectionWindow();
        UiChecks checks(app, smoke);
        int frame = 0;
        std::string previousTitle;
        while (!WindowShouldClose()) {
            float scale = std::min(GetScreenWidth() / 1280.f, GetScreenHeight() / 800.f);
            Vector2 offset = {(GetScreenWidth() - 1280 * scale) / 2, (GetScreenHeight() - 800 * scale) / 2};
            Vector2 pointer = GetMousePosition();
            app.ui.pointer = {{(pointer.x - offset.x) / scale, (pointer.y - offset.y) / scale},
                              IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                              IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                              IsMouseButtonReleased(MOUSE_BUTTON_LEFT)};
            checks.beforeFrame();
            app.update(std::min(GetFrameTime(), .05f));
            BeginTextureMode(canvasTexture);
            app.render();
            EndTextureMode();
            BeginDrawing();
            ClearBackground({22, 24, 27, 255});
            DrawTexturePro(canvasTexture.texture, {0, 0, 1280, -800}, {offset.x, offset.y, 1280 * scale, 800 * scale},
                           {0, 0}, 0, WHITE);
            EndDrawing();
            if (!hidden)
                processSceneDialogs(app, GetWindowHandle());
            if (app.library) {
                auto title =
                    "Light Lab 2D - " + app.library->title(app.scene) + (app.library->modified(app.scene) ? " *" : "");
                if (title != previousTitle) {
                    SetWindowTitle(title.c_str());
                    previousTitle = title;
                }
            }
            detectorWindows.update(app);
            checks.afterFrame();
            ++frame;
            if (checks.done()) {
                result = checks.result();
                if (result == 0)
                    result = runDetectorWindowChecks(app, detectorWindows, GetWindowHandle());
                break;
            }
            if (!capture.empty() && frame >= 4) {
                TakeScreenshot(capture.c_str());
                detectorWindows.captureFirst(capture + ".object.bmp");
                break;
            }
        }
        app.saveSession();
        detectorWindows.closeAll();
        app.ui.unload();
    }
    UnloadRenderTexture(canvasTexture);
    CloseWindow();
    return result;
}
} // namespace lightlab
