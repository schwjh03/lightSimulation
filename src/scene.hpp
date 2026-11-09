#pragma once

#include "raylib.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace lightlab {
inline constexpr float worldWidth = 1000;
inline constexpr float worldHeight = 704;
inline constexpr Rectangle canvas = {280, 72, worldWidth, worldHeight};
inline constexpr std::size_t maxLasers = 16;

enum class Material { Mirror, Absorber, Splitter, Detector };
enum class Glass { Flint, Crown, Water };
enum class EmitterMode { Beam, Point };
enum class Preset { Room, Relay, Blank, Dispersion, SplitterLab };

struct Prism {
    Vector2 center = {500, 260};
    float radius = 105;
    float angle = 57;
    float apex = 60;
    Glass glass = Glass::Flint;
    std::uint64_t id = 0;
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

Scene makeScene(Preset preset);
bool saveScene(const Scene& scene, const std::string& path);
std::string sceneData(const Scene& scene);
bool loadScene(Scene& scene, const std::string& path);
Vector2 keepInside(Vector2 point);
float surfaceLength(const Surface& surface);
std::array<Vector2, 3> prismVertices(const Prism& prism);
bool insidePrism(Vector2 point, const Prism& prism);
Prism fitPrism(Prism prism);
const char* glassName(Glass glass);
const char* materialName(Material material);
float prismCornerOffset(const Prism& prism, int corner);
} // namespace lightlab
