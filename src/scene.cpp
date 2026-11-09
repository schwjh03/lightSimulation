#include "scene.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>

namespace lightlab {
Scene makeScene(Preset preset) {
    Scene scene;
    scene.preset = preset;
    if (preset == Preset::Room) {
        scene.surfaces = {{{820, 150}, {820, 530}, Material::Mirror},
                          {{350, 120}, {640, 120}, Material::Mirror},
                          {{510, 565}, {705, 435}, Material::Mirror},
                          {{290, 620}, {655, 620}, Material::Absorber}};
    } else if (preset == Preset::Relay) {
        scene.lasers[0].position = {170, 450};
        scene.lasers[0].angle = 0;
        scene.lasers[0].spread = 12;
        scene.lasers[0].rayCount = 45;
        scene.surfaces = {{{550, 530}, {710, 370}, Material::Mirror},
                          {{550, 180}, {710, 340}, Material::Mirror},
                          {{210, 180}, {210, 340}, Material::Absorber}};
    } else if (preset == Preset::Dispersion) {
        scene.lasers[0].position = {110, 205};
        scene.lasers[0].angle = 0;
        scene.lasers[0].spread = 0;
        scene.lasers[0].rayCount = 1;
        scene.lasers[0].colorIndex = 5;
        scene.prisms.push_back({{420, 205}, 112, 57});
        scene.surfaces.push_back({{480, 620}, {770, 620}, Material::Detector});
    } else if (preset == Preset::SplitterLab) {
        scene.lasers[0].position = {120, 340};
        scene.lasers[0].angle = 0;
        scene.lasers[0].rayCount = 1;
        scene.lasers[0].colorIndex = 5;
        scene.surfaces = {{{450, 440}, {650, 240}, Material::Splitter, .5f},
                          {{460, 120}, {640, 120}, Material::Detector},
                          {{680, 250}, {680, 430}, Material::Detector}};
    } else {
        scene.lasers[0].position = {500, 352};
        scene.lasers[0].angle = 0;
    }
    return scene;
}

Vector2 keepInside(Vector2 point) {
    return {std::clamp(point.x, 12.f, worldWidth - 12), std::clamp(point.y, 12.f, worldHeight - 12)};
}

float surfaceLength(const Surface& surface) {
    return Vector2Distance(surface.start, surface.end);
}

std::array<Vector2, 3> prismVertices(const Prism& prism) {
    std::array<Vector2, 3> points;
    for (int i = 0; i < 3; ++i) {
        float angle = (prism.angle + prismCornerOffset(prism, i)) * DEG2RAD;
        points[i] = Vector2Add(prism.center, {std::cos(angle) * prism.radius, std::sin(angle) * prism.radius});
    }
    return points;
}

bool insidePrism(Vector2 point, const Prism& prism) {
    auto vertices = prismVertices(prism);
    return CheckCollisionPointTriangle(point, vertices[0], vertices[1], vertices[2]);
}

Prism fitPrism(Prism prism) {
    prism.radius = std::clamp(prism.radius, 24.f, 210.f);
    prism.angle = std::remainder(prism.angle, 360.f);
    prism.apex = std::clamp(prism.apex, 20.f, 100.f);
    auto vertices = prismVertices(prism);
    Vector2 lo = vertices[0], hi = vertices[0];
    for (auto p : vertices) {
        lo = Vector2Min(lo, p);
        hi = Vector2Max(hi, p);
    }
    prism.center.x += std::clamp(0.f, 12 - lo.x, worldWidth - 12 - hi.x);
    prism.center.y += std::clamp(0.f, 12 - lo.y, worldHeight - 12 - hi.y);
    return prism;
}

float prismCornerOffset(const Prism& prism, int corner) {
    return corner == 0 ? 0.f : corner == 1 ? 180.f - prism.apex : 180.f + prism.apex;
}

const char* glassName(Glass glass) {
    switch (glass) {
    case Glass::Flint:
        return "N-SF11 flint";
    case Glass::Crown:
        return "N-BK7 crown";
    default:
        return "Water model";
    }
}

const char* materialName(Material material) {
    switch (material) {
    case Material::Mirror:
        return "Mirror";
    case Material::Absorber:
        return "Wall";
    case Material::Splitter:
        return "Splitter";
    default:
        return "Detector";
    }
}

std::string sceneData(const Scene& scene) {
    std::ostringstream file;
    file << std::setprecision(std::numeric_limits<float>::max_digits10);
    file << "LIGHT_LAB_2D 4\n" << scene.lasers.size() << '\n';
    for (const Emitter& light : scene.lasers) {
        file << light.position.x << ' ' << light.position.y << ' ' << light.angle << ' ' << light.spread << ' '
             << light.rayCount << ' ' << light.bounces << ' ' << light.colorIndex << ' ' << static_cast<int>(light.mode)
             << ' ' << light.sweep << '\n';
    }
    file << static_cast<int>(scene.preset) << ' ' << scene.grid << ' ' << scene.normals << ' ' << scene.surfaces.size()
         << '\n';
    for (const Surface& surface : scene.surfaces) {
        file << surface.start.x << ' ' << surface.start.y << ' ' << surface.end.x << ' ' << surface.end.y << ' '
             << static_cast<int>(surface.material) << ' ' << surface.reflectivity << '\n';
    }
    file << scene.prisms.size() << '\n';
    for (const auto& prism : scene.prisms) {
        file << prism.center.x << ' ' << prism.center.y << ' ' << prism.radius << ' ' << prism.angle << ' '
             << prism.apex << ' ' << static_cast<int>(prism.glass) << '\n';
    }
    file << scene.partialReflections << '\n';
    return file.str();
}

bool saveScene(const Scene& scene, const std::string& path) {
    std::ofstream file(std::filesystem::u8path(path));
    file << sceneData(scene);
    file.close();
    return static_cast<bool>(file);
}

bool loadScene(Scene& scene, const std::string& path) {
    std::ifstream file(std::filesystem::u8path(path));
    std::string magic;
    int version, mode, preset;
    size_t count;
    Scene loaded;
    if (!(file >> magic >> version) || magic != "LIGHT_LAB_2D" || version < 1 || version > 4)
        return false;
    auto validPoint = [](Vector2 point) {
        return std::isfinite(point.x) && std::isfinite(point.y) && point.x >= 0 && point.x <= worldWidth &&
               point.y >= 0 && point.y <= worldHeight;
    };
    size_t laserCount = 1;
    if (version >= 4 && (!(file >> laserCount) || laserCount > maxLasers))
        return false;
    loaded.lasers.clear();
    for (size_t i = 0; i < laserCount; ++i) {
        Emitter light;
        if (!(file >> light.position.x >> light.position.y >> light.angle >> light.spread >> light.rayCount >>
              light.bounces >> light.colorIndex >> mode))
            return false;
        if (version >= 4 && !(file >> light.sweep))
            return false;
        if (!validPoint(light.position) || !std::isfinite(light.angle) || !std::isfinite(light.spread) ||
            light.angle < -180 || light.angle > 180 || light.spread < 0 || light.spread > 120 || light.rayCount < 1 ||
            light.rayCount > 240 || light.bounces < 0 || light.bounces > 12 || light.colorIndex < 0 ||
            light.colorIndex > 6 || mode < 0 || mode > 1)
            return false;
        light.mode = static_cast<EmitterMode>(mode);
        loaded.lasers.push_back(light);
    }
    bool legacySweep = false;
    if (!(file >> preset >> loaded.grid >> loaded.normals))
        return false;
    if (version < 4 && !(file >> legacySweep))
        return false;
    if (!(file >> count) || count > 64 || preset < 0 || preset > (version == 1 ? 2 : version == 2 ? 3 : 4))
        return false;
    if (version < 4)
        loaded.lasers[0].sweep = legacySweep;
    loaded.preset = static_cast<Preset>(preset);
    for (size_t index = 0; index < count; ++index) {
        Surface surface;
        int material;
        if (!(file >> surface.start.x >> surface.start.y >> surface.end.x >> surface.end.y >> material) ||
            !validPoint(surface.start) || !validPoint(surface.end) || surfaceLength(surface) < 8 || material < 0 ||
            material > (version < 3 ? 1 : 3)) {
            return false;
        }
        surface.material = static_cast<Material>(material);
        if (version >= 3 && (!(file >> surface.reflectivity) || !std::isfinite(surface.reflectivity) ||
                             surface.reflectivity < 0 || surface.reflectivity > 1))
            return false;
        loaded.surfaces.push_back(surface);
    }
    if (version >= 2) {
        if (!(file >> count) || count > 16)
            return false;
        for (size_t i = 0; i < count; ++i) {
            Prism prism;
            if (!(file >> prism.center.x >> prism.center.y >> prism.radius >> prism.angle) ||
                !validPoint(prism.center) || !std::isfinite(prism.radius) || !std::isfinite(prism.angle) ||
                prism.radius < 24 || prism.radius > 210 || std::abs(prism.angle) > 360)
                return false;
            if (version >= 3) {
                int glass;
                if (!(file >> prism.apex >> glass) || !std::isfinite(prism.apex) || prism.apex < 20 ||
                    prism.apex > 100 || glass < 0 || glass > 2)
                    return false;
                prism.glass = static_cast<Glass>(glass);
            }
            for (auto vertex : prismVertices(prism)) {
                if (!validPoint(vertex))
                    return false;
            }
            loaded.prisms.push_back(prism);
        }
    }
    bool legacyMotion;
    if (version >= 3 && !(file >> loaded.partialReflections))
        return false;
    if (version == 3 && !(file >> legacyMotion))
        return false;
    scene = loaded;
    return true;
}
} // namespace lightlab
