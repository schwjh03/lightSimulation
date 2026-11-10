#include "app.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace lightlab {
App::App(bool testSession) : testing(testSession) {
    if (testing) {
        return;
    }
#ifdef _WIN32
    const char* appData = std::getenv("LOCALAPPDATA");
    auto directory =
        appData ? std::filesystem::path(appData) / "LightLab2D" : std::filesystem::path(GetApplicationDirectory());
#else
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    auto directory = dataHome && *dataHome && std::filesystem::path(dataHome).is_absolute()
        ? std::filesystem::path(dataHome) / "LightLab2D"
        : (home ? std::filesystem::path(home) / ".local/share/LightLab2D"
                : std::filesystem::path(GetApplicationDirectory()) / "LightLab2D");
#endif
    library = std::make_unique<SceneLibrary>(directory);
    if (library->restore(scene)) {
        status = "Scene restored";
    }
    selected = scene.lasers.empty() ? -1 : -2;
    selectedLaser = scene.lasers.empty() ? -1 : 0;
}

void App::save() {
    if (!library || library->currentPath().empty()) {
        saveRequested = true;
        return;
    }
    status = library->preserve(scene) ? "Scene saved" : library->error();
    statusTime = 3;
}

void App::saveSession() {
    if (library)
        library->finish(scene);
}

bool App::saveNamed(const std::string& path) {
    bool ok = library && library->saveAs(scene, std::filesystem::u8path(path));
    status = ok ? "Scene saved" : library ? library->error() : "No scene library";
    statusTime = 4;
    return ok;
}

bool App::openNamed(const std::string& path) {
    if (!library)
        return false;
    Scene validated;
    if (!loadScene(validated, path)) {
        status = "Invalid scene file";
        statusTime = 4;
        return false;
    }
    if (!library->preserve(scene)) {
        status = library->error();
        statusTime = 4;
        return false;
    }
    checkpoint();
    if (!library->open(scene, std::filesystem::u8path(path))) {
        history.pop_back();
        status = library->error();
        statusTime = 4;
        return false;
    }
    selected = scene.lasers.empty() ? -1 : -2;
    selectedLaser = scene.lasers.empty() ? -1 : 0;
    selectedPrism = -1;
    tool = Tool::Select;
    drag = Drag::None;
    ensureSurfaceIds();
    status = "Scene opened";
    statusTime = 3;
    return true;
}

void App::checkpoint() {
    ensureSurfaceIds();
    if (history.size() >= 40) {
        history.erase(history.begin());
    }
    history.push_back({scene, library ? library->context() : SceneContext{}});
}

void App::undo() {
    if (history.empty()) {
        return;
    }
    if (library && library->context().path != history.back().context.path && !library->preserve(scene)) {
        status = library->error();
        statusTime = 4;
        return;
    }
    scene = history.back().scene;
    if (library)
        library->restoreContext(history.back().context);
    history.pop_back();
    selected = -1;
    selectedPrism = -1;
    drag = Drag::None;
}

void App::ensureSurfaceIds() {
    for (auto& light : scene.lasers)
        if (!light.id)
            light.id = nextSurfaceId++;
    for (auto& prism : scene.prisms)
        if (!prism.id)
            prism.id = nextSurfaceId++;
    for (auto& surface : scene.surfaces) {
        if (surface.id == 0)
            surface.id = nextSurfaceId++;
    }
}

void App::requestSelectionWindow() {
    ensureSurfaceIds();
    if (selected == -2 && selectedLaser >= 0 && selectedLaser < static_cast<int>(scene.lasers.size()))
        detectorWindowRequests.push_back(scene.lasers[selectedLaser].id);
    else if (selected == -3 && selectedPrism >= 0 && selectedPrism < static_cast<int>(scene.prisms.size()))
        detectorWindowRequests.push_back(scene.prisms[selectedPrism].id);
    else if (selected >= 0 && selected < static_cast<int>(scene.surfaces.size()))
        detectorWindowRequests.push_back(scene.surfaces[selected].id);
}

void App::requestDetectorWindow(int surfaceIndex) {
    ensureSurfaceIds();
    if (surfaceIndex >= 0 && surfaceIndex < static_cast<int>(scene.surfaces.size()) &&
        scene.surfaces[surfaceIndex].material == Material::Detector) {
        detectorWindowRequests.push_back(scene.surfaces[surfaceIndex].id);
    }
}

void App::eraseSelection() {
    if (selected == -2 && selectedLaser >= 0 && selectedLaser < static_cast<int>(scene.lasers.size())) {
        checkpoint();
        scene.lasers.erase(scene.lasers.begin() + selectedLaser);
        selected = -1;
        selectedLaser = -1;
        drag = Drag::None;
        return;
    }
    if (selected == -3 && selectedPrism >= 0 && selectedPrism < static_cast<int>(scene.prisms.size())) {
        checkpoint();
        scene.prisms.erase(scene.prisms.begin() + selectedPrism);
        selectedPrism = -1;
        selected = -1;
        drag = Drag::None;
        return;
    }
    if (selected < 0 || selected >= static_cast<int>(scene.surfaces.size())) {
        return;
    }
    checkpoint();
    scene.surfaces.erase(scene.surfaces.begin() + selected);
    selected = -1;
    drag = Drag::None;
}

void App::choosePreset(Preset preset) {
    if (library && !library->preserve(scene)) {
        status = library->error();
        statusTime = 5;
        return;
    }
    checkpoint();
    scene = makeScene(preset);
    if (library)
        library->begin(scene);
    selected = scene.lasers.empty() ? -1 : -2;
    selectedLaser = scene.lasers.empty() ? -1 : 0;
    selectedPrism = -1;
    tool = Tool::Select;
    drag = Drag::None;
}

void App::handleCanvas() {
    const Pointer& pointer = ui.pointer;
    Vector2 point = keepInside({pointer.position.x - canvas.x, pointer.position.y - canvas.y});
    if (pointer.pressed && ui.hovered(canvas)) {
        anchor = point;
        pointerMoved = false;
        if (tool == Tool::Laser) {
            drag = Drag::CreateLaser;
            draftEnd = point;
        } else if (tool == Tool::Prism) {
            drag = Drag::CreatePrism;
            draftEnd = point;
        } else if (tool != Tool::Select) {
            drag = Drag::Create;
            draftEnd = point;
        } else {
            int activeLaser = selected == -2 ? selectedLaser : -1;
            selected = -1;
            selectedPrism = -1;
            drag = Drag::None;
            if (activeLaser >= 0 && activeLaser < static_cast<int>(scene.lasers.size())) {
                const auto& light = scene.lasers[activeLaser];
                Vector2 aim =
                    Vector2Add(light.position,
                               Vector2Scale({std::cos(light.angle * DEG2RAD), std::sin(light.angle * DEG2RAD)}, 46));
                if (light.mode == EmitterMode::Beam && Vector2Distance(point, aim) < 10) {
                    selected = -2;
                    selectedLaser = activeLaser;
                    drag = Drag::Direction;
                }
            }
            for (int i = static_cast<int>(scene.lasers.size()) - 1; i >= 0 && drag == Drag::None; --i) {
                const auto& light = scene.lasers[i];
                Vector2 direction{std::cos(light.angle * DEG2RAD), std::sin(light.angle * DEG2RAD)};
                Vector2 relative = Vector2Subtract(point, light.position);
                float along = Vector2DotProduct(relative, direction);
                float across = std::abs(relative.x * direction.y - relative.y * direction.x);
                bool onBody = light.mode == EmitterMode::Beam && along >= -54 && along <= 6 && across < 17;
                if (Vector2Distance(point, light.position) < 17 || onBody) {
                    selected = -2;
                    selectedLaser = i;
                    drag = Drag::Source;
                    sourceGrabOffset = Vector2Subtract(light.position, point);
                }
            }
            if (drag == Drag::None) {
                for (int i = static_cast<int>(scene.surfaces.size()) - 1; i >= 0; --i) {
                    const auto& surface = scene.surfaces[i];
                    if (Vector2Distance(point, surface.start) < 11)
                        drag = Drag::FirstEnd;
                    else if (Vector2Distance(point, surface.end) < 11)
                        drag = Drag::SecondEnd;
                    else if (CheckCollisionPointLine(point, surface.start, surface.end, 8))
                        drag = Drag::Surface;
                    else
                        continue;
                    selected = i;
                    original = surface;
                    break;
                }
                if (selected == -1) {
                    for (int i = static_cast<int>(scene.prisms.size()) - 1; i >= 0; --i) {
                        const auto& prism = scene.prisms[i];
                        auto vertices = prismVertices(prism);
                        int corner = -1;
                        for (int j = 0; j < 3; ++j)
                            if (Vector2Distance(point, vertices[j]) < 13)
                                corner = j;
                        if (corner < 0 && !insidePrism(point, prism) && Vector2Distance(point, prism.center) >= 12)
                            continue;
                        selected = -3;
                        selectedPrism = i;
                        originalPrism = prism;
                        prismCorner = std::max(0, corner);
                        drag = corner < 0 ? Drag::PrismMove : Drag::PrismCorner;
                        break;
                    }
                }
            }
            if (drag != Drag::None) {
                checkpoint();
            }
        }
    }

    if ((pointer.down || pointer.released) && Vector2Distance(anchor, point) > 5)
        pointerMoved = true;
    if (pointer.down) {
        if (drag == Drag::Source && selectedLaser >= 0 && selectedLaser < static_cast<int>(scene.lasers.size())) {
            scene.lasers[selectedLaser].position = keepInside(Vector2Add(point, sourceGrabOffset));
        }
        if (drag == Drag::Direction && selectedLaser >= 0 && selectedLaser < static_cast<int>(scene.lasers.size())) {
            Vector2 direction = Vector2Subtract(point, scene.lasers[selectedLaser].position);
            scene.lasers[selectedLaser].angle = std::atan2(direction.y, direction.x) * RAD2DEG;
        }
        if (drag == Drag::Create || drag == Drag::CreatePrism || drag == Drag::CreateLaser) {
            draftEnd = point;
        }
        if (selected == -3 && selectedPrism >= 0) {
            auto& prism = scene.prisms[selectedPrism];
            if (drag == Drag::PrismMove) {
                prism = originalPrism;
                prism.center = Vector2Add(originalPrism.center, Vector2Subtract(point, anchor));
                prism = fitPrism(prism);
            } else if (drag == Drag::PrismCorner) {
                auto direction = Vector2Subtract(point, prism.center);
                if (Vector2Length(direction) >= 24) {
                    prism.radius = Vector2Length(direction);
                    prism.angle =
                        std::atan2(direction.y, direction.x) * RAD2DEG - prismCornerOffset(prism, prismCorner);
                    prism = fitPrism(prism);
                }
            }
        }
        if (selected >= 0) {
            auto& surface = scene.surfaces[selected];
            if (drag == Drag::FirstEnd && Vector2Distance(point, surface.end) >= 8) {
                surface.start = point;
            }
            if (drag == Drag::SecondEnd && Vector2Distance(point, surface.start) >= 8) {
                surface.end = point;
            }
            if (drag == Drag::Surface) {
                Vector2 delta = Vector2Subtract(point, anchor);
                delta.x = std::clamp(delta.x, 12 - std::min(original.start.x, original.end.x),
                                     worldWidth - 12 - std::max(original.start.x, original.end.x));
                delta.y = std::clamp(delta.y, 12 - std::min(original.start.y, original.end.y),
                                     worldHeight - 12 - std::max(original.start.y, original.end.y));
                surface.start = Vector2Add(original.start, delta);
                surface.end = Vector2Add(original.end, delta);
            }
        }
    }
    if (pointer.released) {
        if (drag == Drag::CreateLaser) {
            if (scene.lasers.size() < maxLasers) {
                checkpoint();
                Emitter light;
                light.position = anchor;
                light.angle = 0;
                light.colorIndex = 1;
                light.rayCount = 1;
                auto direction = Vector2Subtract(point, anchor);
                if (Vector2Length(direction) >= 12)
                    light.angle = std::atan2(direction.y, direction.x) * RAD2DEG;
                scene.lasers.push_back(light);
                selected = -2;
                selectedLaser = static_cast<int>(scene.lasers.size()) - 1;
                selectedPrism = -1;
                tool = Tool::Select;
            } else {
                status = "Scene limit: 16 lasers";
                statusTime = 3;
                drag = Drag::None;
            }
        }
        if (drag == Drag::CreatePrism && scene.prisms.size() < 16) {
            checkpoint();
            Prism prism;
            prism.center = anchor;
            auto offset = Vector2Subtract(point, anchor);
            if (Vector2Length(offset) >= 24) {
                prism.radius = Vector2Length(offset);
                prism.angle = std::atan2(offset.y, offset.x) * RAD2DEG;
            }
            scene.prisms.push_back(fitPrism(prism));
            selected = -3;
            selectedPrism = static_cast<int>(scene.prisms.size()) - 1;
            tool = Tool::Select;
            status = "Prism placed. Drag its body to move; corners resize and rotate.";
            statusTime = 5;
        }
        if (drag == Drag::Create && Vector2Distance(anchor, point) >= 16 && scene.surfaces.size() < 64) {
            checkpoint();
            Material material = tool == Tool::Mirror     ? Material::Mirror
                                : tool == Tool::Splitter ? Material::Splitter
                                : tool == Tool::Detector ? Material::Detector
                                                         : Material::Absorber;
            scene.surfaces.push_back({anchor, point, material});
            selected = static_cast<int>(scene.surfaces.size()) - 1;
            selectedPrism = -1;
            if (tool == Tool::Splitter || tool == Tool::Detector)
                tool = Tool::Select;
        }
        std::uint64_t clickedId = 0;
        if (!pointerMoved && tool == Tool::Select && drag != Drag::None &&
            drag != Drag::Create && drag != Drag::CreateLaser && drag != Drag::CreatePrism) {
            ensureSurfaceIds();
            if (selected == -2 && selectedLaser >= 0)
                clickedId = scene.lasers[selectedLaser].id;
            else if (selected == -3 && selectedPrism >= 0)
                clickedId = scene.prisms[selectedPrism].id;
            else if (selected >= 0)
                clickedId = scene.surfaces[selected].id;
        }
        double now = GetTime();
        if (clickedId && clickedId == lastObjectClickId && now - lastObjectClickTime <= .4 &&
            Vector2Distance(point, lastObjectClickPosition) <= 5) {
            requestSelectionWindow();
            lastObjectClickId = 0;
        } else {
            lastObjectClickId = clickedId;
        }
        lastObjectClickTime = now;
        lastObjectClickPosition = point;
        drag = Drag::None;
    }
}

void App::update(float deltaTime) {
    if (scene.grid)
        backgroundTime += deltaTime;
    statusTime = std::max(0.f, statusTime - deltaTime);
    if (IsKeyPressed(KEY_F11)) {
        ToggleBorderlessWindowed();
    }
    bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if (control && IsKeyPressed(KEY_Z)) {
        undo();
    }
    if (control && IsKeyPressed(KEY_S)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
            saveAsRequested = true;
        else
            save();
    }
    if (control && IsKeyPressed(KEY_O))
        openRequested = true;
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        eraseSelection();
    }
    if (IsKeyPressed(KEY_V)) {
        tool = Tool::Select;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_M)) {
        tool = Tool::Mirror;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_B)) {
        tool = Tool::Wall;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_P)) {
        tool = Tool::Prism;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_S) && !control) {
        tool = Tool::Splitter;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_D)) {
        tool = Tool::Detector;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_L) && !control) {
        tool = Tool::Laser;
        drag = Drag::None;
    }
    if (IsKeyPressed(KEY_G)) {
        checkpoint();
        scene.grid = !scene.grid;
    }
    if (IsKeyPressed(KEY_N)) {
        checkpoint();
        scene.normals = !scene.normals;
    }
    if (IsKeyPressed(KEY_SPACE) && selected == -2 && selectedLaser >= 0 &&
        selectedLaser < static_cast<int>(scene.lasers.size())) {
        checkpoint();
        scene.lasers[selectedLaser].sweep = !scene.lasers[selectedLaser].sweep;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        drag = Drag::None;
        tool = Tool::Select;
    }
    handleCanvas();
    for (size_t i = 0; i < scene.lasers.size(); ++i) {
        auto& light = scene.lasers[i];
        bool aiming =
            selected == -2 && selectedLaser == static_cast<int>(i) && (drag == Drag::Source || drag == Drag::Direction);
        if (light.sweep && !aiming) {
            light.angle += deltaTime * 24;
            if (light.angle > 180)
                light.angle -= 360;
        }
    }
}
} // namespace lightlab
