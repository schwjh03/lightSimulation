#include "app.hpp"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace lightlab {
namespace {
Vector2 screen(Vector2 point) {
    return {canvas.x + point.x, canvas.y + point.y};
}

const Color background = {8, 14, 24, 255};

Color surfaceTint(Material material) {
    return material == Material::Mirror     ? mirrorColor
           : material == Material::Splitter ? splitterColor
           : material == Material::Detector ? detectorColor
                                            : wallColor;
}

void drawGlass(const Prism& prism, bool selected, bool preview = false) {
    auto points = prismVertices(prism);
    auto a = screen(points[0]), b = screen(points[1]), c = screen(points[2]);
    Vector2 shadow{4, 7};
    DrawTriangle(Vector2Add(a, shadow), Vector2Add(c, shadow), Vector2Add(b, shadow), Fade(BLACK, .28f));
    DrawTriangle(a, c, b, Color{85, 131, 217, static_cast<unsigned char>(preview ? 14 : 28)});
    const auto center = screen(prism.center);
    DrawTriangle(a, center, b, Color{172, 214, 255, 18});
    DrawTriangle(b, center, c, Color{57, 97, 176, 26});
    for (int i = 0; i < 3; ++i) {
        auto start = screen(points[i]), end = screen(points[(i + 1) % 3]);
        DrawLineEx(start, end, selected ? 6.f : 4.f, Fade(prismColor, .1f));
        DrawLineEx(start, end, 1.5f, Fade(prismColor, preview ? .5f : .88f));
        auto innerA = Vector2Lerp(start, center, .035f), innerB = Vector2Lerp(end, center, .035f);
        DrawLineEx(innerA, innerB, 1, Fade(WHITE, .16f));
        if (selected || preview) {
            DrawCircleV(start, 7, paper);
            DrawCircleLinesV(start, 7, prismColor);
            DrawCircleV(start, 2, WHITE);
        }
    }
    if (selected) {
        DrawCircleV(center, 9, paper);
        DrawLineEx({center.x - 4, center.y}, {center.x + 4, center.y}, 1, prismColor);
        DrawLineEx({center.x, center.y - 4}, {center.x, center.y + 4}, 1, prismColor);
    }
}

void drawHardware(const Surface& surface, bool selected) {
    auto a = screen(surface.start), b = screen(surface.end);
    auto tangent = Vector2Normalize(Vector2Subtract(b, a));
    Vector2 normal{-tangent.y, tangent.x};
    const bool mirror = surface.material == Material::Mirror;
    const Color tint = surfaceTint(surface.material);
    DrawLineEx(Vector2Add(a, {2, 4}), Vector2Add(b, {2, 4}), 15, Fade(BLACK, .5f));
    if (selected)
        DrawLineEx(a, b, 19, Fade(tint, .12f));
    DrawLineEx(a, b, 12, Color{60, 76, 96, 255});
    DrawLineEx(a, b, 8, mirror ? Color{25, 44, 63, 255} : Color{51, 40, 44, 255});
    if (surface.material == Material::Detector) {
        DrawLineEx(a, b, 8, Color{13, 52, 51, 255});
        DrawLineEx(a, b, 2, Fade(detectorColor, .8f));
    } else if (surface.material == Material::Splitter) {
        DrawLineEx(a, b, 7, Color{72, 46, 98, 255});
        DrawLineEx(a, b, 2, Fade(splitterColor, .8f));
    }
    for (float offset = 8; offset < surfaceLength(surface) - 5; offset += 12) {
        auto p = Vector2Add(a, Vector2Scale(tangent, offset));
        auto start = Vector2Add(p, Vector2Scale(normal, 4));
        auto end = Vector2Add(Vector2Add(p, Vector2Scale(tangent, 4)), Vector2Scale(normal, -3));
        DrawLineEx(start, end, 1, Fade(tint, mirror ? .24f : .48f));
    }
    DrawLineEx(Vector2Add(a, Vector2Scale(normal, -4)), Vector2Add(b, Vector2Scale(normal, -4)), mirror ? 2.f : 1.f,
               mirror ? Color{208, 242, 255, 255} : tint);
    for (auto point : {a, b}) {
        DrawCircleV(point, selected ? 7.f : 4.f, paper);
        DrawCircleLinesV(point, selected ? 7.f : 4.f, tint);
        DrawCircleV(point, selected ? 2.f : 1.f, selected ? WHITE : tint);
    }
}

void drawEmitter(const Emitter& light, bool selected) {
    const Vector2 source = screen(light.position);
    const Color color = lightColors[light.colorIndex];
    const Vector2 direction{std::cos(light.angle * DEG2RAD), std::sin(light.angle * DEG2RAD)};
    const Vector2 normal{-direction.y, direction.x};
    auto along = [&](float distance, float side = 0.f) {
        return Vector2Add(source, Vector2Add(Vector2Scale(direction, distance), Vector2Scale(normal, side)));
    };
    if (light.mode == EmitterMode::Beam) {
        if (selected) {
            DrawLineEx(along(-54), along(-3), 34, Fade(color, .14f));
            auto aim = along(46);
            DrawLineEx(along(8), aim, 1, Fade(color, .4f));
            DrawCircleV(aim, 7, paper);
            DrawCircleLinesV(aim, 7, color);
            DrawCircleV(aim, 2, WHITE);
        }
        DrawLineEx(along(-52, 4), along(-5, 4), 29, Fade(BLACK, .6f));
        DrawLineEx(along(-52), along(-6), 26, Color{90, 106, 127, 255});
        DrawLineEx(along(-51, 2), along(-7, 2), 20, Color{32, 45, 63, 255});
        DrawLineEx(along(-49, -7), along(-10, -7), 2, Color{159, 180, 204, 255});
        for (float d = -44; d < -18; d += 5) {
            DrawLineEx(along(d, -9), along(d, 9), 2, Color{16, 25, 38, 255});
        }
        DrawLineEx(along(-15), along(-5), 25, Color{100, 120, 145, 255});
        DrawLineEx(along(-14, 1), along(-6, 1), 19, Color{47, 64, 86, 255});
        DrawLineEx(along(-5), along(0), 14, Color{151, 171, 194, 255});
        DrawLineEx(along(-3), along(1), 7, color);
        DrawCircleV(along(-20, -4), 2, color);
    } else {
        DrawCircleV(Vector2Add(source, {2, 4}), 20, Fade(BLACK, .5f));
        DrawCircleV(source, 18, Color{64, 83, 106, 255});
        DrawCircleV(source, 14, Color{23, 36, 52, 255});
        DrawCircleV(source, 7, color);
        DrawCircleV(source, 3, WHITE);
        if (selected)
            DrawCircleLinesV(source, 23, Fade(color, .7f));
    }
    DrawCircleGradient(static_cast<int>(source.x), static_cast<int>(source.y), 13, Fade(color, .3f), Fade(color, 0));
}
} // namespace

void App::drawToolbar() {
    DrawRectangle(0, 0, 1280, 72, Color{21, 31, 46, 255});
    DrawLine(0, 71, 1280, 71, border);
    DrawCircle(31, 32, 8, teal);
    DrawLineEx({39, 29}, {60, 18}, 2, teal);
    DrawLineEx({39, 32}, {64, 32}, 2, teal);
    DrawLineEx({39, 35}, {60, 46}, 2, teal);
    ui.text("LIGHT LAB", 78, 12, 28, ink, true);
    ui.text("2D OPTICS", 79, 46, 12, muted, true);

    for (int i = 0; i < 6; ++i) {
        Rectangle rect = {300 + i * 98.f, 20, 92, 34};
        if (ui.button(rect, std::array<const char*, 6>{"Select", "Mirror", "Wall", "Prism", "Splitter", "Detector"}[i],
                      static_cast<int>(tool) == i)) {
            tool = static_cast<Tool>(i);
            drag = Drag::None;
        }
        ui.hint(rect, std::array<const char*, 6>{"Select / move (V)", "Draw mirror (M)", "Draw wall (B)",
                                                 "Place a prism (P). Drag to size and rotate.",
                                                 "Draw a beam splitter (S)", "Draw a light detector (D)"}[i]);
    }

    if (ui.button({898, 20, 58, 34}, "Open"))
        openRequested = true;
    ui.hint({898, 20, 58, 34}, "Open saved scene (Ctrl+O)");
    if (ui.button({962, 20, 58, 34}, "Save"))
        save();
    ui.hint({962, 20, 58, 34}, "Save scene (Ctrl+S)");
    if (ui.button({1026, 20, 80, 34}, "Save as"))
        saveAsRequested = true;
    ui.hint({1026, 20, 80, 34}, "Save a separate named scene (Ctrl+Shift+S)");
    Rectangle undoButton = {1112, 20, 42, 34};
    if (ui.button(undoButton, "", false, !history.empty()))
        undo();
    DrawLineEx({1142, 31}, {1123, 31}, 2, history.empty() ? border : ink);
    DrawLineEx({1123, 31}, {1129, 25}, 2, history.empty() ? border : ink);
    DrawLineEx({1123, 31}, {1129, 37}, 2, history.empty() ? border : ink);
    DrawLineEx({1142, 31}, {1142, 44}, 2, history.empty() ? border : ink);
    ui.hint(undoButton, "Undo (Ctrl+Z)");
    if (ui.toggle({1162, 24, 105, 25}, "Grid", scene.grid)) {
        checkpoint();
        scene.grid = !scene.grid;
    }
}

void App::drawInspector() {
    DrawRectangle(0, 72, 280, 704, paper);
    DrawLine(279, 72, 279, 776, border);
    ui.text("SCENE", 22, 95, 13, muted, true);
    for (int i = 0; i < 3; ++i) {
        if (ui.button({22 + i * 80.f, 119, 76, 30}, std::array<const char*, 3>{"Room", "Relay", "Blank"}[i],
                      (!library || library->currentPath().empty()) && static_cast<int>(scene.preset) == i)) {
            choosePreset(static_cast<Preset>(i));
        }
    }
    if (ui.button({22, 157, 116, 30}, "Prism demo",
                  scene.preset == Preset::Dispersion && (!library || library->currentPath().empty())))
        choosePreset(Preset::Dispersion);
    if (ui.button({144, 157, 114, 30}, "Splitter lab",
                  scene.preset == Preset::SplitterLab && (!library || library->currentPath().empty())))
        choosePreset(Preset::SplitterLab);
    ui.hint({22, 157, 116, 30}, "White light, prism and detector. Current work is saved before switching.");
    ui.hint({144, 157, 114, 30}, "A 50/50 splitter with two detectors. Current work is saved before switching.");
    ui.text("ADD LIGHTS", 22, 208, 13, muted, true);
    if (ui.button({22, 233, 236, 38}, "Add laser (L)", tool == Tool::Laser, scene.lasers.size() < maxLasers)) {
        tool = Tool::Laser;
        drag = Drag::None;
    }
    ui.text("Click the canvas to place a laser.", 22, 285, 15, ink);
    ui.text("Drag while placing to set its direction.", 22, 307, 13, muted);
    ui.text("LASERS", 22, 334, 12, muted, true);
    ui.text(std::to_string(scene.lasers.size()) + " / 16", 207, 334, 13, teal, true);
    const Rectangle list{22, 357, 236, 198};
    if (ui.hovered(list))
        laserListOffset -= static_cast<int>(std::round(GetMouseWheelMove()));
    laserListOffset = std::clamp(laserListOffset, 0, std::max(0, static_cast<int>(scene.lasers.size()) - 5));
    for (int row = 0; row < 5 && row + laserListOffset < static_cast<int>(scene.lasers.size()); ++row) {
        int index = row + laserListOffset;
        const auto& light = scene.lasers[index];
        Rectangle rect{22, 357 + row * 40.f, 236, 34};
        if (ui.button(rect, "", selected == -2 && selectedLaser == index)) {
            selected = -2;
            selectedLaser = index;
            selectedPrism = -1;
            tool = Tool::Select;
            requestSelectionWindow();
        }
        DrawRectangle(static_cast<int>(rect.x + 7), static_cast<int>(rect.y + 8), 4, 18, lightColors[light.colorIndex]);
        ui.text("Laser " + std::to_string(index + 1), rect.x + 21, rect.y + 7, 16, ink, true);
        std::string color = colorNames[light.colorIndex];
        color = color.substr(0, color.find('/'));
        ui.text(color, rect.x + 115, rect.y + 8, 14, lightColors[light.colorIndex]);
        ui.hint(rect, "Open this laser's own settings window.");
    }
    if (scene.lasers.empty())
        ui.text("No lasers. Add one above.", 22, 370, 15, muted);
    ui.text(scene.lasers.size() > 5 ? "Scroll the list to see more lasers." : "Click a laser to edit its own settings.",
            22, 568, 13, muted);
    ui.text("VIEW", 22, 607, 12, muted, true);
    if (ui.toggle({22, 641, 236, 27}, "Surface normals", scene.normals)) {
        checkpoint();
        scene.normals = !scene.normals;
    }
    DrawLine(22, 699, 258, 699, border);
    ui.text("SELECTION", 22, 714, 11, muted, true);
    std::string label = selected == -2 ? "Laser " + std::to_string(selectedLaser + 1) : "None";
    if (selected >= 0) {
        label = std::string(materialName(scene.surfaces[selected].material)) + " " + std::to_string(selected + 1);
    }
    if (selected == -3)
        label = "Glass prism";
    ui.text(label, 22, 735, 21, ink, true);
    if (selected != -1) {
        if (ui.button({145, 733, 65, 25}, "Open"))
            requestSelectionWindow();
        ui.hint({145, 733, 65, 25}, "Open the selected object's separate inspector window.");
    }
    Rectangle remove = {219, 720, 39, 39};
    if (ui.button(remove, "", false, selected != -1)) {
        eraseSelection();
    }
    Color removeColor = selected != -1 ? wallColor : border;
    DrawRectangleLinesEx({231, 734, 15, 16}, 2, removeColor);
    DrawLineEx({228, 729}, {249, 729}, 2, removeColor);
    DrawLineEx({235, 725}, {242, 725}, 2, removeColor);
    ui.hint(remove, "Delete selected object (Del)");
}

void App::drawCanvas() {
    BeginScissorMode(static_cast<int>(canvas.x), static_cast<int>(canvas.y), static_cast<int>(canvas.width),
                     static_cast<int>(canvas.height));
    DrawRectangleGradientV(280, 72, 1000, 704, Color{12, 22, 37, 255}, background);
    if (scene.grid) {
        const float time = static_cast<float>(backgroundTime);
        const float driftX = std::fmod(time * 3.f, 40.f), driftY = std::fmod(time * 1.4f, 40.f);
        for (int x = -40; x <= static_cast<int>(worldWidth) + 40; x += 40) {
            for (int y = -40; y <= static_cast<int>(worldHeight) + 40; y += 40) {
                float px = x + driftX, py = y + driftY;
                float sway = 2.f * std::sin(py * .012f + time * .18f);
                DrawCircleV(screen({px + sway, py}), 1.15f, Color{35, 54, 77, 255});
            }
        }
    }
    for (size_t i = 0; i < scene.prisms.size(); ++i)
        drawGlass(scene.prisms[i], false);
    rays = traceScene(scene);
    readings = readDetectors(scene, rays);
    std::vector<float> densities, spectralGains;
    for (const auto& light : scene.lasers) {
        float effectiveRays =
            light.mode == EmitterMode::Beam && light.spread < .001f ? 1.f : static_cast<float>(light.rayCount);
        densities.push_back(std::min(1.f, std::sqrt(12.f / effectiveRays)));
        spectralGains.push_back(static_cast<float>(sourceSpectrum(light.colorIndex).size()));
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& ray : rays) {
        if (!ray.visible)
            continue;
        const float intensity =
            std::sqrt(std::clamp(ray.energy * (ray.dispersed ? spectralGains[ray.source] : 1.f), 0.f, 1.f)) *
            densities[ray.source];
        auto a = screen(ray.start), b = screen(ray.end);
        DrawLineEx(a, b, 12, Fade(ray.color, intensity * .018f));
        DrawLineEx(a, b, 5, Fade(ray.color, intensity * .08f));
        DrawLineEx(a, b, 1.5f, Fade(ray.color, intensity * .86f));
        if (ray.hitMirror || ray.hitGlass) {
            DrawCircleGradient(static_cast<int>(b.x), static_cast<int>(b.y), 7, Fade(ray.color, intensity * .2f),
                               Fade(ray.color, 0));
        } else {
            DrawCircleV(b, 2, Fade(ray.color, intensity * .7f));
        }
    }
    EndBlendMode();
    if (scene.normals) {
        for (size_t i = 0; i < rays.size(); i += 5) {
            const auto& ray = rays[i];
            if (ray.visible && (ray.hitMirror || ray.hitGlass)) {
                DrawLineEx(screen(ray.end), screen(Vector2Add(ray.end, Vector2Scale(ray.normal, 23))), 1,
                           Fade(ray.hitGlass ? prismColor : mirrorColor, .7f));
            }
        }
    }
    for (size_t i = 0; i < scene.surfaces.size(); ++i) {
        drawHardware(scene.surfaces[i], selected == static_cast<int>(i));
        const auto& surface = scene.surfaces[i];
        auto midpoint = screen(Vector2Scale(Vector2Add(surface.start, surface.end), .5f));
        if (surface.material == Material::Detector) {
            const float power = i < readings.size() ? readings[i].power : 0;
            DrawCircleGradient(static_cast<int>(midpoint.x), static_cast<int>(midpoint.y), 19,
                               Fade(detectorColor, std::min(.35f, power * .35f)), Fade(detectorColor, 0));
            Vector2 badge = midpoint;
            if (std::abs(surface.end.y - surface.start.y) > std::abs(surface.end.x - surface.start.x))
                badge.x -= 68;
            badge.x = std::clamp(badge.x, canvas.x + 56, canvas.x + canvas.width - 56);
            DrawRectangleRounded({badge.x - 52, badge.y + 14, 104, 25}, .2f, 6, paper);
            ui.center(TextFormat("D%u / %.1f%%", static_cast<unsigned>(i + 1), power * 100), badge.x, badge.y + 18, 14,
                      detectorColor, true);
        } else if (surface.material == Material::Splitter) {
            ui.center(TextFormat("%.0f / %.0f", surface.reflectivity * 100, (1 - surface.reflectivity) * 100),
                      midpoint.x, midpoint.y + 13, 12, splitterColor);
        }
    }
    // Glass edges stay crisp; the transparent fill is drawn underneath the rays.
    for (size_t i = 0; i < scene.prisms.size(); ++i) {
        auto vertices = prismVertices(scene.prisms[i]);
        const bool active = selected == -3 && selectedPrism == static_cast<int>(i);
        for (int corner = 0; corner < 3; ++corner) {
            auto a = screen(vertices[corner]), b = screen(vertices[(corner + 1) % 3]);
            DrawLineEx(a, b, 1.25f, Fade(prismColor, .8f));
            if (active) {
                DrawCircleV(a, 7, paper);
                DrawCircleLinesV(a, 7, prismColor);
                DrawCircleV(a, 2, WHITE);
            }
        }
        auto center = screen(scene.prisms[i].center);
        if (active) {
            DrawCircleV(center, 8, paper);
            DrawLineEx({center.x - 4, center.y}, {center.x + 4, center.y}, 1, prismColor);
            DrawLineEx({center.x, center.y - 4}, {center.x, center.y + 4}, 1, prismColor);
        }
        ui.center(glassName(scene.prisms[i].glass), center.x, center.y + 25, 11, Fade(prismColor, .65f));
    }
    if (drag == Drag::Create) {
        const Color color = tool == Tool::Mirror     ? mirrorColor
                            : tool == Tool::Splitter ? splitterColor
                            : tool == Tool::Detector ? detectorColor
                                                     : wallColor;
        DrawLineEx(screen(anchor), screen(draftEnd), 2, Fade(color, .65f));
        DrawCircleLinesV(screen(anchor), 5, color);
        DrawCircleLinesV(screen(draftEnd), 5, color);
    }
    if (drag == Drag::CreatePrism) {
        Prism preview;
        preview.center = anchor;
        auto offset = Vector2Subtract(draftEnd, anchor);
        if (Vector2Length(offset) >= 24) {
            preview.radius = Vector2Length(offset);
            preview.angle = std::atan2(offset.y, offset.x) * RAD2DEG;
        }
        drawGlass(fitPrism(preview), false, true);
    }
    if (drag == Drag::CreateLaser) {
        Emitter preview;
        preview.position = anchor;
        preview.colorIndex = 1;
        auto offset = Vector2Subtract(draftEnd, anchor);
        preview.angle = Vector2Length(offset) < 12 ? 0 : std::atan2(offset.y, offset.x) * RAD2DEG;
        drawEmitter(preview, true);
    }
    for (size_t i = 0; i < scene.lasers.size(); ++i) {
        drawEmitter(scene.lasers[i], selected == -2 && selectedLaser == static_cast<int>(i));
    }

    ui.text("OPTICAL BENCH", 306, 90, 12, muted, true);
    if (selected == -2 && selectedLaser >= 0 && selectedLaser < static_cast<int>(scene.lasers.size())) {
        const auto& light = scene.lasers[selectedLaser];
        ui.text("Laser " + std::to_string(selectedLaser + 1) + " / " + colorNames[light.colorIndex], 306, 112, 16,
                lightColors[light.colorIndex]);
    } else
        ui.text(std::to_string(scene.lasers.size()) + " lasers / click a source for its settings", 306, 112, 16, muted);
    ui.text("MIRROR", 980, 94, 10, mirrorColor, true);
    ui.text("WALL", 1072, 94, 10, wallColor, true);
    ui.text("PRISM", 1154, 94, 10, prismColor, true);
    DrawRectangleGradientV(280, 721, 1000, 55, Fade(background, 0), Fade(background, .94f));
    const char* hint =
        tool == Tool::Laser   ? "Click to add a laser. Drag while placing to aim it. Each laser has its own settings."
        : tool == Tool::Prism ? "Click to place a prism, or drag from its center to set a corner."
        : selected == -3
            ? "PRISM  /  Drag the body to move; corners resize / rotate. Controls open in a separate window."
        : selected >= 0 && scene.surfaces[selected].material == Material::Detector
            ? "DETECTOR  /  Live readings open in a separate window. Drag its title bar anywhere."
            : "L Add laser   V Select   M Mirror   B Wall   P Prism   S Splitter   D Detector";
    ui.text(hint, 306, 747, 14, muted);
    EndScissorMode();
}

void App::drawStatus() {
    DrawRectangle(0, 776, 1280, 24, paper);
    DrawLine(0, 776, 1280, 776, border);
    std::string sceneTitle =
        library ? library->title(scene) + (library->modified(scene) ? " *" : "") : "Spectral optics";
    if (sceneTitle.size() > 34)
        sceneTitle = sceneTitle.substr(0, 31) + "...";
    ui.text(statusTime > 0 ? status : sceneTitle, 22, 780, 13, muted);
    ui.text(std::to_string(scene.surfaces.size()) + " surfaces / " + std::to_string(scene.prisms.size()) + " prisms",
            301, 780, 13, muted);
    ui.text(std::to_string(rays.size()) + " ray segments", 520, 780, 13, muted);
    if (ui.hovered(canvas)) {
        ui.text("x " + std::to_string(static_cast<int>(ui.pointer.position.x - canvas.x)) + "   y " +
                    std::to_string(static_cast<int>(ui.pointer.position.y - canvas.y)),
                1124, 780, 13, muted);
    }
}

void App::render() {
    ensureSurfaceIds();
    ui.begin();
    ClearBackground(paper);
    drawToolbar();
    ensureSurfaceIds();
    drawInspector();
    drawCanvas();
    drawStatus();
    ui.finish();
}
} // namespace lightlab
