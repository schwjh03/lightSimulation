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
Vector2 emitterRayOrigin(const Emitter& light, float fraction) {
    if (light.mode != EmitterMode::Wall)
        return light.position;
    float angle = light.angle * DEG2RAD;
    return Vector2Add(light.position,
                      Vector2Scale({-std::sin(angle), std::cos(angle)}, (fraction - .5f) * light.width));
}

Emitter fitEmitter(Emitter light) {
    if (light.mode != EmitterMode::Wall)
        return light;
    light.width = std::clamp(light.width, 8.f, 400.f);
    light.rayCount = std::clamp(light.rayCount, 2, 240);
    auto a = emitterRayOrigin(light, 0), b = emitterRayOrigin(light, 1);
    auto lo = Vector2Min(a, b), hi = Vector2Max(a, b);
    light.position.x += std::clamp(0.f, 12 - lo.x, worldWidth - 12 - hi.x);
    light.position.y += std::clamp(0.f, 12 - lo.y, worldHeight - 12 - hi.y);
    return light;
}

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
    } else if (preset == Preset::LensTesting) {
        auto& laser = scene.lasers[0];
        laser.position = {100, 352};
        laser.angle = 0;
        laser.spread = 80;
        laser.rayCount = 161;
        laser.bounces = 8;
        scene.partialReflections = false;
        for (int i = 0; i < 6; ++i) {
            Prism lens;
            lens.center = {550, 86.f + i * 106.f};
            lens.radius = 42;
            lens.angle = 0;
            lens.glass = Glass::Crown;
            lens.lens = static_cast<LensShape>(i + 1);
            lens.thickness = lensThickness(lens);
            scene.prisms.push_back(fitPrism(lens));
        }
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

float maximumSurfaceLength() {
    return Vector2Distance({12, 12}, {worldWidth - 12, worldHeight - 12});
}

namespace {
// Signed radii describe the two spherical faces along the local optical x axis.
// Lengths share the world-to-centimeter scale, so geometric intersections and
// curvature normals are unit-consistent. Refraction uses dimensionless index ratios.
void lensProfile(const Prism& p, float& left, float& right, float& r1, float& r2) {
    float h = p.radius;
    r1 = 1.65f * h;
    r2 = -r1;
    float thickness = .85f * h;
    switch (p.lens) {
    case LensShape::PlanoConvex:
        r1 = 0;
        thickness = .5f * h;
        break;
    case LensShape::Biconcave:
        r1 = -r1;
        r2 = -r2;
        thickness = .16f * h;
        break;
    case LensShape::PlanoConcave:
        r1 = 0;
        r2 = -r2;
        thickness = .16f * h;
        break;
    case LensShape::PositiveMeniscus:
        r2 = 2.8f * h;
        thickness = .4f * h;
        break;
    case LensShape::NegativeMeniscus:
        r1 = 2.8f * h;
        r2 = 1.65f * h;
        thickness = .22f * h;
        break;
    default:
        break;
    }
    if (p.thickness > 0)
        thickness = std::clamp(p.thickness, minimumLensThickness(p), 300.f);
    left = -thickness / 2;
    right = thickness / 2;
}

float faceX(float vertex, float radius, float y) {
    if (radius == 0)
        return vertex;
    // Rationalized spherical sag avoids subtracting nearly equal radii near the axis.
    const double r = radius, height = y;
    return static_cast<float>(vertex + height * height / (r + std::copysign(std::sqrt(r * r - height * height), r)));
}

Vector2 lensLocal(const Prism& p, Vector2 v, bool position) {
    if (position)
        v = Vector2Subtract(v, p.center);
    v = Vector2Rotate(v, -p.angle * DEG2RAD);
    if (p.flipped)
        v.x = -v.x;
    return v;
}

Vector2 lensWorld(const Prism& p, Vector2 v, bool position) {
    if (p.flipped)
        v.x = -v.x;
    v = Vector2Rotate(v, p.angle * DEG2RAD);
    return position ? Vector2Add(v, p.center) : v;
}
}

float minimumLensThickness(const Prism& lens) {
    const float h = lens.radius;
    auto sag = [&](float radius) { return radius - std::sqrt(radius * radius - h * h); };
    if (lens.lens == LensShape::Biconvex)
        return 2 * sag(1.65f * h) + 2;
    if (lens.lens == LensShape::PlanoConvex)
        return sag(1.65f * h) + 2;
    if (lens.lens == LensShape::PositiveMeniscus)
        return sag(1.65f * h) - sag(2.8f * h) + 2;
    return 2;
}

float lensThickness(const Prism& lens) {
    float left, right, r1, r2;
    lensProfile(lens, left, right, r1, r2);
    return right - left;
}

std::array<float, 2> lensRadiiCm(const Prism& lens) {
    float left, right, r1, r2;
    lensProfile(lens, left, right, r1, r2);
    if (lens.flipped) {
        float old = r1;
        r1 = -r2;
        r2 = -old;
    }
    auto radius = [](float r) { return r == 0 ? std::numeric_limits<float>::infinity() : toCentimeters(r); };
    return {radius(r1), radius(r2)};
}

const char* lensName(LensShape shape) {
    static const char* names[] = {"Glass prism",   "Biconvex",     "Plano-Convex", "Biconcave",
                                  "Plano-Concave", "Meniscus (+)", "Meniscus (-)"};
    return names[static_cast<int>(shape)];
}

std::vector<Vector2> prismVertices(const Prism& prism) {
    if (prism.lens != LensShape::Prism) {
        float l, r, r1, r2;
        lensProfile(prism, l, r, r1, r2);
        std::vector<Vector2> points;
        for (int side = 0; side < 2; ++side)
            for (int i = 0; i <= 96; ++i) {
                float y = prism.radius * (-1 + 2.f * (side ? 96 - i : i) / 96);
                points.push_back(lensWorld(prism, {faceX(side ? r : l, side ? r2 : r1, y), y}, true));
            }
        return points;
    }
    std::vector<Vector2> points(3);
    for (int i = 0; i < 3; ++i) {
        float angle = (prism.angle + prismCornerOffset(prism, i)) * DEG2RAD;
        points[i] = Vector2Add(prism.center, {std::cos(angle) * prism.radius, std::sin(angle) * prism.radius});
    }
    return points;
}

bool insidePrism(Vector2 point, const Prism& prism) {
    if (prism.lens != LensShape::Prism) {
        auto q = lensLocal(prism, point, true);
        if (std::abs(q.y) > prism.radius)
            return false;
        float l, r, r1, r2;
        lensProfile(prism, l, r, r1, r2);
        return q.x >= faceX(l, r1, q.y) && q.x <= faceX(r, r2, q.y);
    }
    auto vertices = prismVertices(prism);
    return CheckCollisionPointTriangle(point, vertices[0], vertices[1], vertices[2]);
}

bool intersectLens(const Prism& p, Vector2 origin, Vector2 direction, float& distance, Vector2& normal) {
    auto o = lensLocal(p, origin, true), d = lensLocal(p, direction, false);
    float l, r, r1, r2;
    lensProfile(p, l, r, r1, r2);
    bool found = false;
    auto accept = [&](float t, Vector2 n) {
        if (t < .005f || t >= distance)
            return;
        distance = t;
        normal = lensWorld(p, Vector2Normalize(n), false);
        found = true;
    };
    for (int side = 0; side < 2; ++side) {
        float v = side ? r : l, rad = side ? r2 : r1;
        if (rad == 0) {
            if (std::abs(d.x) > 1e-7f) {
                float t = (v - o.x) / d.x;
                if (std::abs(o.y + t * d.y) <= p.radius)
                    accept(t, {1, 0});
            }
        } else {
            Vector2 c{v + rad, 0}, oc = Vector2Subtract(o, c);
            const double a = static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y;
            const double b = static_cast<double>(oc.x) * d.x + static_cast<double>(oc.y) * d.y;
            const double quadraticC =
                static_cast<double>(oc.x) * oc.x + static_cast<double>(oc.y) * oc.y - static_cast<double>(rad) * rad;
            const double disc = b * b - a * quadraticC;
            if (disc < 0)
                continue;
            for (double root : {(-b - std::sqrt(disc)) / a, (-b + std::sqrt(disc)) / a}) {
                float t = static_cast<float>(root);
                auto q = Vector2Add(o, Vector2Scale(d, t));
                if (std::abs(q.y) <= p.radius && std::abs(q.x - faceX(v, rad, q.y)) < .01f)
                    accept(t, Vector2Subtract(q, c));
            }
        }
    }
    // The finite aperture has physical top and bottom glass edges too.
    if (std::abs(d.y) > 1e-7f)
        for (float y : {-p.radius, p.radius}) {
            float t = (y - o.y) / d.y, x = o.x + t * d.x;
            if (x >= faceX(l, r1, y) && x <= faceX(r, r2, y))
                accept(t, {0, 1});
        }
    return found;
}

Prism fitPrism(Prism prism) {
    prism.radius = std::clamp(prism.radius, 24.f, 210.f);
    if (prism.lens != LensShape::Prism && prism.thickness > 0)
        prism.thickness = std::clamp(prism.thickness, minimumLensThickness(prism), 300.f);
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
    file << "LIGHT_LAB_2D 8\n" << scene.lasers.size() << '\n';
    for (const Emitter& light : scene.lasers) {
        file << light.position.x << ' ' << light.position.y << ' ' << light.angle << ' ' << light.spread << ' '
             << light.rayCount << ' ' << light.bounces << ' ' << light.colorIndex << ' ' << static_cast<int>(light.mode)
             << ' ' << light.sweep << ' ' << light.width << '\n';
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
             << prism.apex << ' ' << static_cast<int>(prism.glass) << ' ' << static_cast<int>(prism.lens) << ' '
             << prism.flipped << ' ' << prism.customIndex << ' ' << prism.thickness << '\n';
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
    if (!(file >> magic >> version) || magic != "LIGHT_LAB_2D" || version < 1 || version > 8)
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
        if (version >= 8 &&
            (!(file >> light.width) || !std::isfinite(light.width) || light.width < 8 || light.width > 400))
            return false;
        if (!validPoint(light.position) || !std::isfinite(light.angle) || !std::isfinite(light.spread) ||
            light.angle < -180 || light.angle > 180 || light.spread < 0 || light.spread > 120 || light.rayCount < 1 ||
            light.rayCount > 240 || light.bounces < 0 || light.bounces > 12 || light.colorIndex < 0 ||
            light.colorIndex > 6 || mode < 0 || mode > (version >= 8 ? 2 : 1))
            return false;
        light.mode = static_cast<EmitterMode>(mode);
        if (light.mode == EmitterMode::Wall &&
            (light.rayCount < 2 || !validPoint(emitterRayOrigin(light, 0)) || !validPoint(emitterRayOrigin(light, 1))))
            return false;
        loaded.lasers.push_back(light);
    }
    bool legacySweep = false;
    if (!(file >> preset >> loaded.grid >> loaded.normals))
        return false;
    if (version < 4 && !(file >> legacySweep))
        return false;
    if (!(file >> count) || count > 64 || preset < 0 ||
        preset > (version == 1   ? 2
                  : version == 2 ? 3
                  : version < 7  ? 4
                                 : 5))
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
            if (version >= 5) {
                int shape;
                if (!(file >> shape >> prism.flipped) || shape < 0 || shape > 6)
                    return false;
                prism.lens = static_cast<LensShape>(shape);
            }
            if (version >= 6 && (!(file >> prism.customIndex) || !std::isfinite(prism.customIndex) ||
                                 (prism.customIndex != 0 && (prism.customIndex < 1 || prism.customIndex > 2.5f))))
                return false;
            if (version >= 7 && (!(file >> prism.thickness) || !std::isfinite(prism.thickness) || prism.thickness < 0 ||
                                 prism.thickness > 300 ||
                                 (prism.lens != LensShape::Prism && prism.thickness > 0 &&
                                  prism.thickness < minimumLensThickness(prism))))
                return false;
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
