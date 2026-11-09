#include "optics.hpp"
#include "raymath.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace lightlab {
std::vector<SpectralBand> sourceSpectrum(int colorIndex) {
    switch (colorIndex) {
    case 0:
        return {{589, 1}}; // A yellow laser, not an RGB screen mixture.
    case 1:
        return {{650, 1}};
    case 2:
        return {{532, 1}};
    case 3:
        return {{485, .5f}, {532, .5f}}; // Cyan: blue + green.
    case 4:
        return {{440, .5f}, {650, .5f}}; // Purple is not a single spectral hue.
    case 5: {
        std::vector<SpectralBand> bands;
        for (int nm = 420; nm <= 700; nm += 20)
            bands.push_back({static_cast<float>(nm), 1.f / 15});
        return bands;
    }
    default:
        return {{460, 1}};
    }
}

Color wavelengthColor(float wavelength) {
    float r = 0, g = 0, b = 0;
    if (wavelength < 440) {
        r = (440 - wavelength) / 60;
        b = 1;
    } else if (wavelength < 490) {
        g = (wavelength - 440) / 50;
        b = 1;
    } else if (wavelength < 510) {
        g = 1;
        b = (510 - wavelength) / 20;
    } else if (wavelength < 580) {
        r = (wavelength - 510) / 70;
        g = 1;
    } else if (wavelength < 645) {
        r = 1;
        g = (645 - wavelength) / 65;
    } else
        r = 1;
    return {static_cast<unsigned char>(255 * std::clamp(r, 0.f, 1.f)),
            static_cast<unsigned char>(255 * std::clamp(g, 0.f, 1.f)),
            static_cast<unsigned char>(255 * std::clamp(b, 0.f, 1.f)), 255};
}

float glassIndex(float wavelength, Glass glass) {
    const double lambda = std::clamp(wavelength, 400.f, 750.f) / 1000.0;
    const double l2 = lambda * lambda;
    if (glass == Glass::Water)
        return static_cast<float>(1.324 + .003 / l2); // Illustrative Cauchy water model.
    if (glass == Glass::Crown) {
        // SCHOTT N-BK7 Sellmeier coefficients; lambda in micrometres.
        return static_cast<float>(std::sqrt(1 + 1.03961212 * l2 / (l2 - .00600069867) +
                                            .231792344 * l2 / (l2 - .0200179144) +
                                            1.01046945 * l2 / (l2 - 103.560653)));
    }
    // SCHOTT N-SF11 Sellmeier coefficients.
    return static_cast<float>(std::sqrt(1 + 1.73759695 * l2 / (l2 - .013188707) + .313747346 * l2 / (l2 - .0623068142) +
                                        1.89878101 * l2 / (l2 - 155.23629)));
}

bool refractDirection(Vector2 incoming, Vector2 normal, float fromIndex, float toIndex, Vector2& outgoing) {
    incoming = Vector2Normalize(incoming);
    normal = Vector2Normalize(normal);
    const float cosine = std::clamp(-Vector2DotProduct(incoming, normal), 0.f, 1.f);
    const float ratio = fromIndex / toIndex;
    const float discriminant = 1 - ratio * ratio * (1 - cosine * cosine);
    if (discriminant < 0)
        return false;
    outgoing = Vector2Normalize(
        Vector2Add(Vector2Scale(incoming, ratio), Vector2Scale(normal, ratio * cosine - std::sqrt(discriminant))));
    return true;
}

float fresnelReflectance(Vector2 incoming, Vector2 normal, float n1, float n2) {
    if (std::abs(n1 - n2) < .00001f)
        return 0;
    const float ci = std::clamp(-Vector2DotProduct(Vector2Normalize(incoming), Vector2Normalize(normal)), 0.f, 1.f);
    const float st2 = (n1 / n2) * (n1 / n2) * (1 - ci * ci);
    if (st2 >= 1)
        return 1;
    const float ct = std::sqrt(1 - st2);
    const float rs = (n1 * ci - n2 * ct) / (n1 * ci + n2 * ct);
    const float rp = (n2 * ci - n1 * ct) / (n2 * ci + n1 * ct);
    return std::clamp((rs * rs + rp * rp) / 2, 0.f, 1.f);
}

std::vector<RaySegment> traceRay(Vector2 origin, Vector2 direction, const std::vector<Surface>& surfaces,
                                 int maxBounces, const std::vector<Prism>& prisms, float wavelength,
                                 bool partialReflections) {
    std::vector<RaySegment> path;
    if (Vector2LengthSqr(direction) < .000001f)
        return path;
    constexpr float farDistance = 4000, epsilon = .005f, minimumEnergy = .0005f;
    constexpr size_t maxSegments = 256;
    const std::array<Surface, 4> boundaries = {{{{0, 0}, {worldWidth, 0}, Material::Absorber},
                                                {{worldWidth, 0}, {worldWidth, worldHeight}, Material::Absorber},
                                                {{worldWidth, worldHeight}, {0, worldHeight}, Material::Absorber},
                                                {{0, worldHeight}, {0, 0}, Material::Absorber}}};
    auto mediumAt = [&](Vector2 point) {
        for (int i = static_cast<int>(prisms.size()) - 1; i >= 0; --i)
            if (insidePrism(point, prisms[i]))
                return i;
        return -1;
    };
    auto indexFor = [&](int medium) { return medium < 0 ? 1.f : glassIndex(wavelength, prisms[medium].glass); };

    struct State {
        Vector2 origin, direction;
        float energy;
        int reflections, steps, medium;
        bool dispersed;
    };

    const int initialMedium = mediumAt(origin);
    std::vector<State> pending = {{origin, Vector2Normalize(direction), 1, 0, 0, initialMedium, initialMedium >= 0}};
    maxBounces = std::clamp(maxBounces, 0, 12);
    while (!pending.empty() && path.size() < maxSegments) {
        auto state = pending.back();
        pending.pop_back();
        while (state.energy >= minimumEnergy && state.steps++ < 64 && path.size() < maxSegments) {
            Vector2 closest = Vector2Add(state.origin, Vector2Scale(state.direction, farDistance)), normal{};
            float nearestDistance = farDistance;
            Material material = Material::Absorber;
            bool hitGlass = false, found = false;
            int hitSurface = -1;
            auto consider = [&](Vector2 a, Vector2 b, Material kind, bool glassEdge, int surfaceIndex) {
                Vector2 point{};
                auto end = Vector2Add(state.origin, Vector2Scale(state.direction, farDistance));
                if (!CheckCollisionLines(state.origin, end, a, b, &point))
                    return;
                float distance = Vector2Distance(state.origin, point);
                bool absorbing = kind == Material::Absorber || kind == Material::Detector;
                bool tieAbsorber = !glassEdge && absorbing && std::abs(distance - nearestDistance) < .001f;
                if (distance < epsilon || (distance >= nearestDistance && !tieAbsorber))
                    return;
                nearestDistance = distance;
                closest = point;
                material = kind;
                hitGlass = glassEdge;
                hitSurface = surfaceIndex;
                found = true;
                const auto tangent = Vector2Subtract(b, a);
                normal = Vector2Normalize({-tangent.y, tangent.x});
                if (Vector2DotProduct(normal, state.direction) > 0)
                    normal = Vector2Negate(normal);
            };
            for (const auto& boundary : boundaries)
                consider(boundary.start, boundary.end, boundary.material, false, -1);
            for (const auto& prism : prisms) {
                auto vertices = prismVertices(prism);
                for (int edge = 0; edge < 3; ++edge)
                    consider(vertices[edge], vertices[(edge + 1) % 3], Material::Mirror, true, -1);
            }
            for (size_t i = 0; i < surfaces.size(); ++i)
                consider(surfaces[i].start, surfaces[i].end, surfaces[i].material, false, static_cast<int>(i));
            if (!found)
                break;
            RaySegment segment{state.origin,
                               closest,
                               normal,
                               state.energy,
                               !hitGlass && (material == Material::Mirror || material == Material::Splitter),
                               hitGlass,
                               state.dispersed,
                               wavelength,
                               wavelengthColor(wavelength)};
            segment.hitSurface = hitSurface;
            path.push_back(segment);
            if (!hitGlass && (material == Material::Absorber || material == Material::Detector))
                break;
            auto spawnReflection = [&](float fraction) {
                if (state.reflections >= maxBounces || state.energy * fraction < minimumEnergy)
                    return;
                auto reflected = Vector2Normalize(Vector2Reflect(state.direction, normal));
                pending.push_back({Vector2Add(closest, Vector2Scale(reflected, .02f)), reflected,
                                   state.energy * fraction, state.reflections + 1, state.steps, state.medium,
                                   state.dispersed});
            };
            Vector2 outgoing = state.direction;
            if (hitGlass) {
                int nextMedium = mediumAt(Vector2Add(closest, Vector2Scale(state.direction, .03f)));
                const float n1 = indexFor(state.medium), n2 = indexFor(nextMedium);
                if (refractDirection(state.direction, normal, n1, n2, outgoing)) {
                    float reflectance = partialReflections ? fresnelReflectance(state.direction, normal, n1, n2) : 0;
                    spawnReflection(reflectance);
                    state.energy *= 1 - reflectance;
                    if (std::abs(n1 - n2) > .00001f)
                        state.dispersed = true;
                    state.medium = nextMedium;
                } else {
                    if (state.reflections++ >= maxBounces)
                        break;
                    outgoing = Vector2Reflect(state.direction, normal);
                }
            } else if (material == Material::Splitter) {
                const float reflectance = std::clamp(surfaces[hitSurface].reflectivity, 0.f, 1.f);
                spawnReflection(reflectance);
                state.energy *= 1 - reflectance;
            } else {
                if (state.reflections++ >= maxBounces)
                    break;
                outgoing = Vector2Reflect(state.direction, normal);
                state.energy *= .82f;
            }
            state.direction = Vector2Normalize(outgoing);
            state.origin = Vector2Add(closest, Vector2Scale(state.direction, .02f));
        }
    }
    return path;
}

std::vector<RaySegment> traceScene(const Scene& scene) {
    std::vector<RaySegment> rays;
    const Color sourceColors[] = {{255, 215, 50, 255},  {255, 48, 69, 255},   {63, 255, 121, 255}, {37, 233, 255, 255},
                                  {200, 102, 255, 255}, {246, 250, 255, 255}, {60, 125, 255, 255}};
    for (size_t source = 0; source < scene.lasers.size() && rays.size() < 120000; ++source) {
        const auto& light = scene.lasers[source];
        const bool narrow = light.mode == EmitterMode::Beam && light.spread < .001f;
        const int rayCount = narrow ? 1 : std::clamp(light.rayCount, 1, 240);
        const auto spectrum = sourceSpectrum(light.colorIndex);
        for (int index = 0; index < rayCount && rays.size() < 120000; ++index) {
            const float fraction = rayCount == 1 ? .5f : index / static_cast<float>(rayCount - 1);
            const float angle = (light.angle + (light.mode == EmitterMode::Point ? index * 360.f / rayCount
                                                                                 : (fraction - .5f) * light.spread)) *
                                DEG2RAD;
            for (size_t band = 0; band < spectrum.size() && rays.size() < 120000; ++band) {
                auto path = traceRay(light.position, {std::cos(angle), std::sin(angle)}, scene.surfaces, light.bounces,
                                     scene.prisms, spectrum[band].wavelength, scene.partialReflections);
                for (auto segment : path) {
                    segment.source = source;
                    segment.power = segment.energy * spectrum[band].weight / rayCount / scene.lasers.size();
                    if (!segment.dispersed) {
                        segment.visible = band == 0;
                        segment.color = sourceColors[std::clamp(light.colorIndex, 0, 6)];
                    } else
                        segment.energy *= spectrum[band].weight;
                    rays.push_back(segment);
                }
            }
        }
    }
    return rays;
}

std::vector<DetectorReading> readDetectors(const Scene& scene, const std::vector<RaySegment>& rays) {
    std::vector<DetectorReading> readings(scene.surfaces.size());
    for (const auto& ray : rays) {
        if (ray.hitSurface < 0 || ray.hitSurface >= static_cast<int>(scene.surfaces.size()) ||
            scene.surfaces[ray.hitSurface].material != Material::Detector)
            continue;
        auto& reading = readings[ray.hitSurface];
        reading.power += ray.power;
        auto band = std::find_if(reading.spectrum.begin(), reading.spectrum.end(), [&](const SpectralBand& value) {
            return std::abs(value.wavelength - ray.wavelength) < .1f;
        });
        if (band == reading.spectrum.end())
            reading.spectrum.push_back({ray.wavelength, ray.power});
        else
            band->weight += ray.power;
    }
    for (auto& reading : readings) {
        std::sort(reading.spectrum.begin(), reading.spectrum.end(),
                  [](auto a, auto b) { return a.wavelength < b.wavelength; });
        float maximum = 0;
        for (auto band : reading.spectrum)
            if (band.weight > maximum) {
                maximum = band.weight;
                reading.peakWavelength = band.wavelength;
            }
    }
    return readings;
}
} // namespace lightlab
