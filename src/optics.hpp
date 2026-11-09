#pragma once

#include "scene.hpp"

namespace lightlab {
struct RaySegment {
    Vector2 start;
    Vector2 end;
    Vector2 normal;
    float energy = 1;
    bool hitMirror = false;
    bool hitGlass = false;
    bool dispersed = false;
    float wavelength = 550;
    Color color = WHITE;
    int hitSurface = -1;
    float power = 0;
    bool visible = true;
    std::size_t source = 0;
};

struct SpectralBand {
    float wavelength;
    float weight;
};

struct DetectorReading {
    float power = 0;
    float peakWavelength = 0;
    std::vector<SpectralBand> spectrum;
};

std::vector<SpectralBand> sourceSpectrum(int colorIndex);
Color wavelengthColor(float wavelength);
float glassIndex(float wavelength, Glass glass = Glass::Flint);
float fresnelReflectance(Vector2 incoming, Vector2 normal, float fromIndex, float toIndex);
bool refractDirection(Vector2 incoming, Vector2 opposingNormal, float fromIndex, float toIndex, Vector2& outgoing);

// A bounded ray path through mirrors and glass, ending at an absorber or canvas edge.
std::vector<RaySegment> traceRay(Vector2 origin, Vector2 direction, const std::vector<Surface>& surfaces,
                                 int maxBounces, const std::vector<Prism>& prisms = {}, float wavelength = 550,
                                 bool partialReflections = false);
std::vector<RaySegment> traceScene(const Scene& scene);
std::vector<DetectorReading> readDetectors(const Scene& scene, const std::vector<RaySegment>& rays);
} // namespace lightlab
