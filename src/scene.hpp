#pragma once

#include "raylib.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace lightlab {
// All stored positions, curvature radii and thicknesses share this fixed scale.
// Rendering uses world units; physical length controls use centimeters.
inline constexpr float worldUnitsPerCentimeter = 10;

constexpr float toCentimeters(float worldLength) {
    return worldLength / worldUnitsPerCentimeter;
}

constexpr float fromCentimeters(float centimeters) {
    return centimeters * worldUnitsPerCentimeter;
}

inline constexpr float worldWidth = 1000;
inline constexpr float worldHeight = 704;
inline constexpr Rectangle canvas = {280, 72, worldWidth, worldHeight};
inline constexpr std::size_t maxLasers = 16;

enum class Material { Mirror, Absorber, Splitter, Detector };
enum class Glass { Flint, Crown, Water };
enum class EmitterMode { Beam, Point, Wall };
enum class Preset { Room, Relay, Blank, Dispersion, SplitterLab, LensTesting };

enum class LensShape { Prism, Biconvex, PlanoConvex, Biconcave, PlanoConcave, PositiveMeniscus, NegativeMeniscus };
const char* lensName(LensShape shape);

struct Prism {
    Vector2 center = {500, 260};
    float radius = 105;
    float angle = 57;
    float apex = 60;
    Glass glass = Glass::Flint;
    std::uint64_t id = 0;
    LensShape lens = LensShape::Prism;
    bool flipped = false;
    float thickness = 0;   // Center thickness in world units; zero keeps legacy proportions.
    float customIndex = 0; // Zero uses the material model; otherwise n at 589 nm.
};

struct Surface {
    Vector2 start;
    Vector2 end;
    Material material = Material::Mirror;
    float reflectivity = .5f;
    std::uint64_t id = 0; // Runtime identity survives undo and surface-index changes.
};

struct Emitter {
    Vector2 position = {280, 340};
    float angle = -10;
    float spread = 0;
    int rayCount = 85;
    int bounces = 6;
    int colorIndex = 0;
    EmitterMode mode = EmitterMode::Beam;
    bool sweep = false;
    float width = 80; // Full laser-wall aperture in world units.
    std::uint64_t id = 0;
};

struct Scene {
    std::vector<Emitter> lasers{Emitter{}};
    std::vector<Surface> surfaces;
    std::vector<Prism> prisms;
    Preset preset = Preset::Room;
    bool grid = true;
    bool normals = false;
    bool partialReflections = true;
};

Emitter fitEmitter(Emitter light);
Vector2 emitterRayOrigin(const Emitter& light, float fraction);
Scene makeScene(Preset preset);
bool saveScene(const Scene& scene, const std::string& path);
std::string sceneData(const Scene& scene);
bool loadScene(Scene& scene, const std::string& path);
Vector2 keepInside(Vector2 point);
float surfaceLength(const Surface& surface);
float maximumSurfaceLength();
std::vector<Vector2> prismVertices(const Prism& prism);
float lensThickness(const Prism& lens);
std::array<float, 2> lensRadiiCm(const Prism& lens); // Signed R1/R2 for left-to-right light; infinity means flat.
float minimumLensThickness(const Prism& lens);
bool intersectLens(const Prism& lens, Vector2 origin, Vector2 direction, float& distance, Vector2& normal);
bool insidePrism(Vector2 point, const Prism& prism);
Prism fitPrism(Prism prism);
const char* glassName(Glass glass);
const char* materialName(Material material);
float prismCornerOffset(const Prism& prism, int corner);
} // namespace lightlab
