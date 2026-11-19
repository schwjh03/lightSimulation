#include "optics.hpp"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace lightlab;
int checks = 0;

void check(bool passed, const std::string& label) {
    ++checks;
    if (!passed) {
        throw std::runtime_error(label);
    }
}

bool near(Vector2 actual, Vector2 expected) {
    return Vector2Distance(actual, expected) < .1f;
}

void academicChecks() {
    // Independent paraxial prediction from the thick-lens matrix in air.
    // Positive radii have their centers of curvature to the right.
    for (int shape = 1; shape <= 6; ++shape)
        for (float halfLength : {60.f, 100.f})
            for (Glass glass : {Glass::Crown, Glass::Flint})
                for (float wavelength : {486.1f, 587.6f, 656.3f})
                    for (bool flipped : {false, true})
                        for (float extraThickness : {0.f, 30.f}) {
                            Prism lens;
                            lens.center = {500, 350};
                            lens.angle = 0;
                            lens.radius = halfLength;
                            lens.lens = static_cast<LensShape>(shape);
                            lens.glass = glass;
                            lens.flipped = flipped;
                            lens.thickness = lensThickness(lens) + extraThickness;
                            double r1 = 1.65 * toCentimeters(halfLength), r2 = -r1;
                            if (shape == 2)
                                r1 = 0;
                            if (shape == 3) {
                                r1 = -r1;
                                r2 = -r2;
                            }
                            if (shape == 4) {
                                r1 = 0;
                                r2 = -r2;
                            }
                            if (shape == 5)
                                r2 = 2.8 * toCentimeters(halfLength);
                            if (shape == 6) {
                                r1 = 2.8 * toCentimeters(halfLength);
                                r2 = 1.65 * toCentimeters(halfLength);
                            }
                            if (flipped) {
                                double old = r1;
                                r1 = -r2;
                                r2 = -old;
                            }
                            double c1 = r1 == 0 ? 0 : 1 / r1, c2 = r2 == 0 ? 0 : 1 / r2;
                            double n = glassIndex(wavelength, glass), d = toCentimeters(lensThickness(lens));
                            double power = (n - 1) * (c1 - c2 + (n - 1) * d * c1 * c2 / n);
                            double expected = (1 - (n - 1) * d * c1 / n) / power;
                            check(std::abs(paraxialFocalLengthCm(lens, wavelength) - 1 / power) < .001,
                                  "inspector focal readout matches the independent signed-radius formula");
                            auto rays = traceRay({100, 350.5f}, {1, 0}, {}, 12, {lens}, wavelength, false);
                            check(rays.size() == 3,
                                  "academic matrix: entry and exit for every lens/material/orientation");
                            auto ray = rays.back();
                            auto out = Vector2Normalize(Vector2Subtract(ray.end, ray.start));
                            double crossing = ray.start.x + (350 - ray.start.y) * out.x / out.y;
                            double measured = toCentimeters(crossing - lens.center.x - lensThickness(lens) / 2);
                            check(std::abs(measured - expected) < std::max(.05, .005 * std::abs(expected)),
                                  "academic matrix: back focal distance agrees within 0.05 cm or 0.5 percent");
                            auto reverse = traceRay(Vector2Add(ray.start, Vector2Scale(out, 1)), Vector2Negate(out), {},
                                                    12, {lens}, wavelength, false);
                            check(reverse.size() == 3, "academic matrix: reverse ray crosses the same two faces");
                            auto reverseDirection =
                                Vector2Normalize(Vector2Subtract(reverse.back().end, reverse.back().start));
                            check(Vector2Distance(reverseDirection, {-1, 0}) < .0005f,
                                  "academic matrix: geometric refraction obeys optical reversibility");
                        }
    for (float n : {1.333f, 1.5168f, 1.78472f, 2.5f}) {
        for (int degrees = 0; degrees <= 80; degrees += 5) {
            float angle = degrees * DEG2RAD;
            Vector2 incoming{std::cos(angle), std::sin(angle)}, outgoing;
            check(refractDirection(incoming, {-1, 0}, 1, n, outgoing) &&
                      std::abs(incoming.y - n * outgoing.y) < .000002f,
                  "Snell angle sweep satisfies n1 sin(theta1)=n2 sin(theta2)");
            float reflectance = fresnelReflectance(incoming, {-1, 0}, 1, n);
            check(std::isfinite(reflectance) && reflectance >= 0 && reflectance <= 1,
                  "Fresnel angle sweep stays finite and conserves interface power");
        }
        float critical = std::asin(1 / n);
        Vector2 outgoing;
        float below = critical - .01f * DEG2RAD, above = critical + .01f * DEG2RAD;
        check(refractDirection({std::cos(below), std::sin(below)}, {-1, 0}, n, 1, outgoing) &&
                  !refractDirection({std::cos(above), std::sin(above)}, {-1, 0}, n, 1, outgoing),
              "critical angle transition agrees with asin(n2/n1)");
    }
    for (int shape = 1; shape <= 6; ++shape) {
        Prism lens;
        lens.center = {500, 350};
        lens.angle = 0;
        lens.radius = 80;
        lens.lens = static_cast<LensShape>(shape);
        lens.glass = Glass::Crown;
        auto rays = traceRay({100, 365}, {1, 0}, {}, 12, {lens}, 589, true);
        float delivered = 0;
        for (auto ray : rays)
            if (!ray.hitGlass && !ray.hitMirror)
                delivered += ray.energy;
        check(delivered > .998f && delivered <= 1.0001f,
              "each lens conserves Fresnel power within the documented pruning cutoff");
    }
    for (int source = 0; source < 7; ++source) {
        float weight = 0;
        for (auto band : sourceSpectrum(source))
            weight += band.weight;
        check(std::abs(weight - 1) < .000001f, "every source spectrum has normalized total power");
    }
    // SCHOTT N-SF11 datasheet lines F, d, C; rounded catalog values.
    check(std::abs(glassIndex(486.1f, Glass::Flint) - 1.80651f) < .00003f &&
              std::abs(glassIndex(656.3f, Glass::Flint) - 1.77596f) < .00003f,
          "N-SF11 dispersion agrees with independent blue and red catalog reference lines");
}

int main() {
    try {
        academicChecks();
        for (int shape = 1; shape <= 6; ++shape) {
            Prism lens;
            lens.center = {500, 350};
            lens.radius = 100;
            lens.angle = 0;
            lens.glass = Glass::Crown;
            lens.lens = static_cast<LensShape>(shape);
            for (bool flip : {false, true}) {
                lens.flipped = flip;
                auto path = traceRay({100, 375}, {1, 0}, {}, 6, {lens}, 589, false);
                check(path.size() == 3 && path[0].hitGlass && path[1].hitGlass,
                      "each lens refracts through both faces in either orientation");
                auto out = Vector2Normalize(Vector2Subtract(path.back().end, path.back().start));
                bool converging = shape == 1 || shape == 2 || shape == 5;
                check(converging ? out.y < -.01f : out.y > .01f,
                      "lens power has correct sign before and after flipping");
                check(std::abs(path.back().energy - 1) < .0001f, "transmission conserves energy without reflections");
                auto axial = traceRay({100, 350}, {1, 0}, {}, 6, {lens}, 589, false);
                check(axial.size() == 3 && std::abs(axial.back().end.y - 350) < .01f, "axial ray stays on axis");
                auto missed = traceRay({100, 460}, {1, 0}, {}, 6, {lens}, 589, false);
                check(missed.size() == 1, "rays outside the aperture miss the lens");
                float originalThickness = lensThickness(lens);
                lens.thickness = originalThickness + 60;
                auto thicker = traceRay({100, 375}, {1, 0}, {}, 6, {lens}, 589, false);
                check(thicker.size() == 3 && std::abs(lens.radius - 100) < .001f,
                      "thickness changes preserve aperture and both refracting faces");
                auto thickerOut = Vector2Normalize(Vector2Subtract(thicker.back().end, thicker.back().start));
                auto axisCrossing = [](const RaySegment& ray, Vector2 direction) {
                    return ray.start.x + (350 - ray.start.y) * direction.x / direction.y;
                };
                check(std::abs(axisCrossing(thicker.back(), thickerOut) - axisCrossing(path.back(), out)) > .1f,
                      "physical center thickness changes real or virtual focal position for each lens and flip");
                Scene saved = makeScene(Preset::Blank), loadedLens;
                saved.prisms = {lens};
                check(saveScene(saved, "lens_test.llab") && loadScene(loadedLens, "lens_test.llab") &&
                          loadedLens.prisms[0].lens == lens.lens && loadedLens.prisms[0].flipped == flip &&
                          std::abs(loadedLens.prisms[0].thickness - lens.thickness) < .001f,
                      "lens shape, orientation and thickness survive save and load");
                std::filesystem::remove("lens_test.llab");
                lens.thickness = 0;
            }
        }
        {
            Prism lens;
            lens.center = {500, 350};
            lens.angle = 0;
            lens.lens = LensShape::Biconvex;
            lens.customIndex = 1;
            auto neutral = traceRay({100, 375}, {1, 0}, {}, 6, {lens}, 589, false);
            check(std::abs(neutral.back().end.y - 375) < .01f, "n=1 lens does not bend light");
            lens.customIndex = 1.9f;
            auto strong = traceRay({100, 375}, {1, 0}, {}, 6, {lens}, 589, false);
            check(strong.back().end.y < 375, "custom n controls refraction");
            Scene saved = makeScene(Preset::Blank), loaded;
            saved.prisms = {lens};
            check(saveScene(saved, "index_test.llab") && loadScene(loaded, "index_test.llab") &&
                      std::abs(loaded.prisms[0].customIndex - 1.9f) < .0001f,
                  "custom n survives save/load");
            std::filesystem::remove("index_test.llab");
        }
        {
            auto testing = makeScene(Preset::LensTesting);
            std::set<int> shapes;
            for (const auto& lens : testing.prisms)
                shapes.insert(static_cast<int>(lens.lens));
            check(testing.lasers.size() == 1 && shapes.size() == 6 && !traceScene(testing).empty(),
                  "lens testing starter has every lens variant and one working laser");
            Scene loaded;
            check(saveScene(testing, "lens_testing.lightscene") && loadScene(loaded, "lens_testing.lightscene") &&
                      loaded.preset == Preset::LensTesting && loaded.prisms.size() == 6 && loaded.lasers.size() == 1,
                  "lens testing starter saves and reloads");
            std::filesystem::remove("lens_testing.lightscene");
        }
        {
            check(std::abs(toCentimeters(fromCentimeters(3.25f)) - 3.25f) < .00001f && fromCentimeters(1) == 10 &&
                      toCentimeters(worldWidth) == 100,
                  "centimeter conversions and physical bench scale are consistent");
            Prism lens;
            lens.center = {500, 350};
            lens.radius = fromCentimeters(10);
            lens.angle = 0;
            lens.lens = LensShape::Biconvex;
            lens.glass = Glass::Crown;
            lens.thickness = fromCentimeters(9);
            auto rays = traceRay({100, 351}, {1, 0}, {}, 6, {lens}, 589, false);
            check(rays.size() == 3, "metric test ray crosses both lens faces");
            auto ray = rays.back();
            auto direction = Vector2Normalize(Vector2Subtract(ray.end, ray.start));
            float crossing = ray.start.x + (350 - ray.start.y) * direction.x / direction.y;
            float backFocal = toCentimeters(crossing - lens.center.x - lensThickness(lens) / 2);
            float n = glassIndex(589, Glass::Crown), r = toCentimeters(1.65f * lens.radius);
            float d = toCentimeters(lensThickness(lens));
            float power = (n - 1) * (2 / r - (n - 1) * d / (n * r * r));
            float expectedBackFocal = (1 - (n - 1) * d / (n * r)) / power;
            check(std::abs(backFocal - expectedBackFocal) < .05f,
                  "ray-traced focal distance agrees with thick-lens formula using centimeter radii and thickness");
        }
        {
            Scene wall = makeScene(Preset::Blank);
            wall.lasers[0].position = {100, 350};
            wall.lasers[0].angle = 0;
            wall.lasers[0].mode = EmitterMode::Wall;
            wall.lasers[0].width = fromCentimeters(8);
            wall.lasers[0].rayCount = 9;
            wall.lasers[0].spread = 80;
            wall.partialReflections = false;
            auto rays = traceScene(wall);
            check(rays.size() == 9, "laser wall creates spatially separated beams even with stored fan spread");
            for (int i = 0; i < 9; ++i) {
                check(std::abs(rays[i].start.y - (310 + i * 10)) < .001f &&
                          std::abs(rays[i].start.y - rays[i].end.y) < .001f,
                      "laser wall origins span its centimeter width and all beams are parallel");
            }
            wall.surfaces = {{{800, 100}, {800, 600}, lightlab::Material::Detector}};
            for (int count : {2, 9, 240}) {
                wall.lasers[0].rayCount = count;
                auto read = readDetectors(wall, traceScene(wall));
                check(std::abs(read[0].power - 1) < .00001f, "wall beam count does not change total emitted power");
            }
            wall.surfaces.clear();
            wall.lasers[0].rayCount = 9;
            wall.lasers[0].width = fromCentimeters(2);
            Prism lens;
            lens.center = {500, 350};
            lens.angle = 0;
            lens.radius = 100;
            lens.glass = Glass::Crown;
            lens.lens = LensShape::Biconvex;
            wall.prisms = {lens};
            float r = lensRadiiCm(lens)[0], n = objectIndex(589, lens), d = toCentimeters(lensThickness(lens));
            float back = paraxialFocalLengthCm(lens) * (1 - (n - 1) * d / (n * r));
            float plane = lens.center.x + lensThickness(lens) / 2 + fromCentimeters(back);
            rays = traceScene(wall);
            int exits = 0;
            for (auto ray : rays)
                if (!ray.hitGlass && ray.dispersed) {
                    auto dir = Vector2Normalize(Vector2Subtract(ray.end, ray.start));
                    float height = ray.start.y + (plane - ray.start.x) * dir.y / dir.x;
                    check(std::abs(toCentimeters(height - 350)) < .05f,
                          "parallel near-axis wall beams meet within 0.05 cm at predicted back focal plane");
                    ++exits;
                }
            check(exits == 9, "all wall beams cross a converging lens");
            Scene loaded;
            check(saveScene(wall, "wall_test.lightscene") && loadScene(loaded, "wall_test.lightscene") &&
                      sceneData(wall) == sceneData(loaded),
                  "laser wall mode, width, beam count and lens survive save/load");
            std::filesystem::remove("wall_test.lightscene");
            for (float angle : {0.f, 30.f, 90.f, -90.f, 180.f}) {
                Emitter edge = wall.lasers[0];
                edge.position = {12, 12};
                edge.width = 400;
                edge.angle = angle;
                edge = fitEmitter(edge);
                for (float fraction : {0.f, 1.f}) {
                    auto origin = emitterRayOrigin(edge, fraction);
                    check(origin.x >= 11.99f && origin.y >= 11.99f && origin.x <= worldWidth - 11.99f &&
                              origin.y <= worldHeight - 11.99f,
                          "rotated wall endpoints remain inside the bench");
                }
            }
        }
        auto empty = traceRay({100, 100}, {1, 0}, {}, 6);
        check(empty.size() == 1 && near(empty[0].end, {1000, 100}), "ray terminates at canvas");
        std::vector<Surface> surfaces = {{{500, 50}, {500, 500}, lightlab::Material::Mirror}};
        auto reflection = traceRay({100, 100}, {1, 0}, surfaces, 6);
        check(reflection.size() == 2 && near(reflection[0].end, {500, 100}) && near(reflection[1].end, {0, 100}),
              "vertical mirror reflection");
        check(std::abs(reflection[1].energy - .82f) < .001f, "reflection energy falloff");
        surfaces.push_back({{250, 50}, {250, 500}, lightlab::Material::Absorber});
        auto blocked = traceRay({100, 100}, {1, 0}, surfaces, 6);
        check(blocked.size() == 1 && near(blocked[0].end, {250, 100}), "nearest wall blocks farther mirror");
        auto relay = makeScene(Preset::Relay);
        auto path = traceRay(relay.lasers[0].position, {1, 0}, relay.surfaces, 6);
        check(path.size() == 3, "relay has two reflections");
        check(near(path[0].end, {630, 450}) && near(path[1].end, {630, 260}) && near(path[2].end, {210, 260}),
              "45 degree mirror relay geometry");
        surfaces = {{{300, 50}, {300, 650}, lightlab::Material::Mirror},
                    {{700, 50}, {700, 650}, lightlab::Material::Mirror}};
        auto cavity = traceRay({500, 300}, {1, 0}, surfaces, 3);
        check(cavity.size() == 4 && near(cavity.back().end, {300, 300}), "reflection budget prevents endless bouncing");
        check(traceRay({500, 300}, {1, 0}, surfaces, 0).size() == 1, "zero bounces");
        check(traceRay({500, 300}, {0, 0}, surfaces, 6).empty(), "zero direction rejected");
        auto horizontal = traceRay({100, 100}, {9, 0}, {}, 1);
        check(near(horizontal.back().end, {1000, 100}), "input direction normalized");
        Scene pointScene = makeScene(Preset::Blank);
        pointScene.lasers[0].mode = EmitterMode::Point;
        pointScene.lasers[0].rayCount = 4;
        auto pointRays = traceScene(pointScene);
        check(pointRays.size() == 4 && near(pointRays[0].end, {1000, 352}) && near(pointRays[1].end, {500, 704}),
              "point emitter evenly distributes rays");
        pointScene.lasers[0].mode = EmitterMode::Beam;
        pointScene.lasers[0].rayCount = 1;
        check(near(traceScene(pointScene)[0].end, {1000, 352}), "one-ray beam uses its center angle");
        check(std::abs(glassIndex(587.6f) - 1.78472f) < .0001f, "N-SF11 matches catalog index");
        check(glassIndex(420) > glassIndex(650), "shorter wavelengths have a larger glass index");
        Vector2 refracted;
        const Vector2 incident = Vector2Normalize({1, 1});
        check(refractDirection(incident, {0, -1}, 1, glassIndex(550), refracted) &&
                  std::abs(incident.x - glassIndex(550) * refracted.x) < .0001f,
              "air to glass obeys Snell's law");
        Vector2 emergent;
        check(refractDirection(refracted, {0, -1}, glassIndex(550), 1, emergent) && near(emergent, incident),
              "parallel faces restore the incident direction");
        check(!refractDirection(Vector2Normalize({1, .1f}), {0, -1}, glassIndex(550), 1, emergent),
              "glass to air supports total internal reflection");
        auto demo = makeScene(Preset::Dispersion);
        demo.partialReflections = false; // Isolate primary refraction from Fresnel branches below.
        auto red = traceRay(demo.lasers[0].position, {1, 0}, {}, 0, demo.prisms, 650);
        auto violet = traceRay(demo.lasers[0].position, {1, 0}, {}, 0, demo.prisms, 420);
        check(red.size() == 3 && violet.size() == 3 && red[0].hitGlass && red[1].hitGlass,
              "prism traces entry, exit and emergence without spending reflection budget");
        check(violet.back().end.x < red.back().end.x - 20, "violet deviates more than red in the dispersion demo");
        auto white = traceScene(demo);
        std::set<int> wavelengths;
        int inputSegments = 0;
        for (const auto& ray : white) {
            if (ray.dispersed)
                wavelengths.insert(static_cast<int>(ray.wavelength));
            else if (ray.visible)
                ++inputSegments;
        }
        int visibleSegments =
            static_cast<int>(std::count_if(white.begin(), white.end(), [](auto ray) { return ray.visible; }));
        check(wavelengths.size() == 15 && inputSegments == 1 && visibleSegments == 31,
              "white has fifteen transmitted bands and one unsplit white input");
        check(white[0].color.r > 240 && white[0].color.g > 240 && white[0].color.b > 240,
              "white input stays white before the prism");
        for (int color = 0; color < 7; ++color) {
            demo.lasers[0].colorIndex = color;
            auto rays = traceScene(demo);
            std::set<int> bands;
            for (auto ray : rays)
                if (ray.dispersed)
                    bands.insert(static_cast<int>(ray.wavelength));
            check(bands.size() == sourceSpectrum(color).size(), "each source preserves only its own wavelengths");
        }
        demo.lasers[0].colorIndex = 4;
        check(sourceSpectrum(4).size() == 2 && sourceSpectrum(4)[0].wavelength == 440 &&
                  sourceSpectrum(4)[1].wavelength == 650,
              "purple contains violet and red, not invented green");
        demo.lasers[0].colorIndex = 5;
        demo.surfaces = {{{20, 500}, {980, 500}, lightlab::Material::Absorber}};
        auto absorbedSpectrum = traceScene(demo);
        bool allAbsorbed = true;
        for (auto ray : absorbedSpectrum)
            if (!ray.hitGlass && std::abs(ray.end.y - 500) > .1f)
                allAbsorbed = false;
        check(allAbsorbed, "walls absorb every refracted wavelength");
        demo.surfaces[0].material = lightlab::Material::Mirror;
        bool reflectsSpectrum = false;
        for (auto ray : traceScene(demo))
            reflectsSpectrum |= ray.hitMirror && ray.dispersed;
        check(reflectsSpectrum, "mirrors reflect the dispersed wavelengths");
        auto inside = traceRay(demo.prisms[0].center, {1, 0}, {}, 12, demo.prisms, 532);
        check(!inside.empty() && inside[0].dispersed && inside.size() < 64,
              "a source inside glass traces safely through internal reflections");
        auto shifted = fitPrism({{1, 1}, 500, -420});
        bool fits = shifted.radius == 210 && std::abs(shifted.angle) <= 180;
        for (auto vertex : prismVertices(shifted))
            fits &= vertex.x >= 11.9f && vertex.y >= 11.9f;
        check(fits, "editing constrains every prism vertex to the canvas");
        check(std::abs(glassIndex(587.6f, Glass::Crown) - 1.5168f) < .0001f,
              "crown glass matches the N-BK7 catalog index");
        check(glassIndex(550, Glass::Water) < glassIndex(550, Glass::Crown) &&
                  glassIndex(550, Glass::Crown) < glassIndex(550, Glass::Flint),
              "materials have distinct refractive indices");
        Prism narrow{{400, 300}, 100, 57, 35, Glass::Crown};
        auto corners = prismVertices(narrow);
        auto v1 = Vector2Normalize(Vector2Subtract(corners[1], corners[0]));
        auto v2 = Vector2Normalize(Vector2Subtract(corners[2], corners[0]));
        check(std::abs(std::acos(Vector2DotProduct(v1, v2)) * RAD2DEG - 35) < .01f,
              "apex parameter changes actual prism geometry");
        check(std::abs(fresnelReflectance({1, 0}, {-1, 0}, 1, 1.5f) - .04f) < .0001f,
              "Fresnel reflectance is four percent at normal incidence for n=1.5");
        check(fresnelReflectance(Vector2Normalize({1, .01f}), {0, -1}, 1.5f, 1) == 1,
              "Fresnel total internal reflection returns all power");
        auto branches = traceRay({110, 205}, {1, 0}, {}, 12, makeScene(Preset::Dispersion).prisms, 550, true);
        float terminalEnergy = 0;
        bool hasFaintBranch = false;
        for (const auto& ray : branches) {
            if (!ray.hitGlass && !ray.hitMirror)
                terminalEnergy += ray.energy;
            if (ray.energy < .4f && ray.end.x < ray.start.x)
                hasFaintBranch = true;
        }
        check(hasFaintBranch && terminalEnergy > .995f && terminalEnergy <= 1.0001f,
              "glass produces partial reflections and conserves power to the branch cutoff");
        auto split = makeScene(Preset::SplitterLab);
        auto readings = readDetectors(split, traceScene(split));
        check(std::abs(readings[1].power - .5f) < .0001f && std::abs(readings[2].power - .5f) < .0001f,
              "50/50 splitter delivers half the source power to each detector");
        check(readings[1].spectrum.size() == 15 && readings[2].spectrum.size() == 15,
              "both splitter detectors measure the full white spectrum");
        for (float ratio : {0.f, .25f, .75f, 1.f}) {
            split.surfaces[0].reflectivity = ratio;
            readings = readDetectors(split, traceScene(split));
            check(std::abs(readings[1].power - ratio) < .0001f && std::abs(readings[2].power - (1 - ratio)) < .0001f,
                  "adjustable splitter ratio conserves source power");
        }
        split.surfaces[0].reflectivity = .5f;
        split.lasers[0].rayCount = 240;
        readings = readDetectors(split, traceScene(split));
        check(std::abs(readings[1].power + readings[2].power - 1) < .0001f,
              "coincident ray density does not inflate detector power");
        split.lasers[0].colorIndex = 4;
        readings = readDetectors(split, traceScene(split));
        check(readings[1].spectrum.size() == 2 && readings[1].spectrum[0].wavelength == 440,
              "detectors identify the actual purple source components");
        split.surfaces = {{{450, 440}, {650, 240}, lightlab::Material::Splitter, .5f},
                          {{650, 390}, {750, 290}, lightlab::Material::Splitter, .5f},
                          {{400, 120}, {800, 120}, lightlab::Material::Detector},
                          {{850, 250}, {850, 430}, lightlab::Material::Detector}};
        readings = readDetectors(split, traceScene(split));
        check(std::abs(readings[2].power - .75f) < .0001f && std::abs(readings[3].power - .25f) < .0001f,
              "cascaded splitters add distinct arriving branches without duplicating power");
        split.surfaces.insert(split.surfaces.begin(), {{300, 100}, {300, 600}, lightlab::Material::Absorber});
        readings = readDetectors(split, traceScene(split));
        check(readings[3].power == 0 && readings[4].power == 0, "a blocking wall makes detector readings zero");
        auto measuredPrism = makeScene(Preset::Dispersion);
        readings = readDetectors(measuredPrism, traceScene(measuredPrism));
        check(readings[0].power > .5f && readings[0].power < 1 && readings[0].spectrum.size() == 15,
              "prism detector measures the transmitted spectrum including Fresnel losses");
        Scene metered = makeScene(Preset::Blank);
        metered.lasers[0].mode = EmitterMode::Point;
        metered.lasers[0].rayCount = 4;
        metered.lasers[0].colorIndex = 5;
        metered.surfaces = {{{850, 12}, {850, 692}, lightlab::Material::Detector}};
        auto pointReading = readDetectors(metered, traceScene(metered));
        check(std::abs(pointReading[0].power - .25f) < .0001f,
              "a detector catching one of four point rays measures one quarter of source power");
        metered.lasers[0].mode = EmitterMode::Beam;
        metered.lasers[0].colorIndex = 1;
        metered.surfaces = {{{700, 100}, {700, 600}, lightlab::Material::Mirror},
                            {{200, 300}, {200, 400}, lightlab::Material::Detector}};
        auto mirrorReading = readDetectors(metered, traceScene(metered));
        check(std::abs(mirrorReading[1].power - .82f) < .0001f && mirrorReading[1].spectrum.size() == 1,
              "a detector after a lossy mirror measures the attenuated monochromatic power");
        Scene multiple = makeScene(Preset::Blank);
        multiple.lasers[0].position = {100, 200};
        multiple.lasers[0].colorIndex = 1;
        multiple.lasers[0].rayCount = 1;
        Emitter green;
        green.position = {100, 400};
        green.angle = 0;
        green.colorIndex = 2;
        green.rayCount = 1;
        multiple.lasers.push_back(green);
        multiple.surfaces = {{{800, 100}, {800, 600}, lightlab::Material::Detector}};
        auto bothRays = traceScene(multiple);
        auto bothReading = readDetectors(multiple, bothRays);
        check(bothRays.size() == 2 && bothRays[0].source == 0 && bothRays[1].source == 1 &&
                  bothRays[0].color.r == 255 && bothRays[1].color.g == 255,
              "multiple lasers preserve independent beam origins, colors and source identity");
        check(std::abs(bothReading[0].power - 1) < .0001f && bothReading[0].spectrum.size() == 2 &&
                  std::abs(bothReading[0].spectrum[0].weight - .5f) < .0001f,
              "detectors combine both laser spectra relative to total emitted power");
        multiple.lasers[1].angle = 90;
        auto oneReading = readDetectors(multiple, traceScene(multiple));
        check(std::abs(oneReading[0].power - .5f) < .0001f && oneReading[0].spectrum.size() == 1 &&
                  multiple.lasers[0].angle == 0,
              "aiming one laser away does not alter the other laser");
        multiple.lasers[1].angle = 0;
        multiple.lasers[0].bounces = 0;
        multiple.surfaces = {{{500, 100}, {500, 600}, lightlab::Material::Mirror},
                             {{50, 100}, {50, 600}, lightlab::Material::Detector}};
        auto independentBounces = readDetectors(multiple, traceScene(multiple));
        check(std::abs(independentBounces[1].power - .41f) < .0001f && independentBounces[1].spectrum.size() == 1 &&
                  independentBounces[1].spectrum[0].wavelength == 532,
              "each laser uses its own reflection budget");
        auto mixedPrism = makeScene(Preset::Dispersion);
        Emitter redLight = mixedPrism.lasers[0];
        redLight.colorIndex = 1;
        redLight.id = 0;
        mixedPrism.lasers.push_back(redLight);
        auto mixedReading = readDetectors(mixedPrism, traceScene(mixedPrism));
        check(mixedReading[0].spectrum.size() == 16 && mixedReading[0].power > .5f && mixedReading[0].power <= 1,
              "white and red lasers disperse together without introducing or losing source bands");
        multiple.lasers.clear();
        check(traceScene(multiple).empty() && readDetectors(multiple, traceScene(multiple))[1].power == 0,
              "scenes with no lasers are safe and detector readings are zero");
        const auto savePath = (std::filesystem::temp_directory_path() / "light-lab-test-scene.txt").string();
        check(saveScene(relay, savePath), "save scene");
        Scene loaded;
        check(loadScene(loaded, savePath) && loaded.surfaces.size() == 3 && loaded.preset == Preset::Relay,
              "load scene");
        check(near(loaded.surfaces[0].start, relay.surfaces[0].start), "surface data round trip");
        auto prismScene = makeScene(Preset::Dispersion);
        prismScene.lasers[0].colorIndex = 6;
        check(saveScene(prismScene, savePath) && loadScene(loaded, savePath) && loaded.prisms.size() == 1 &&
                  loaded.lasers[0].colorIndex == 6 && loaded.preset == Preset::Dispersion &&
                  near(loaded.prisms[0].center, prismScene.prisms[0].center),
              "save includes prism geometry, blue source and dispersion preset");
        auto labSave = makeScene(Preset::SplitterLab);
        labSave.surfaces[0].reflectivity = .37f;
        labSave.prisms.push_back(narrow);
        labSave.partialReflections = false;
        labSave.grid = false;
        check(saveScene(labSave, savePath) && loadScene(loaded, savePath) &&
                  std::abs(loaded.surfaces[0].reflectivity - .37f) < .0001f &&
                  loaded.surfaces[1].material == lightlab::Material::Detector &&
                  loaded.prisms[0].glass == Glass::Crown && loaded.prisms[0].apex == 35 && !loaded.partialReflections &&
                  !loaded.grid && loaded.preset == Preset::SplitterLab,
              "scene files preserve detectors, splitter ratios, prism materials, apex angle and grid settings");
        Scene many = makeScene(Preset::Blank);
        many.lasers.clear();
        for (size_t i = 0; i < maxLasers; ++i) {
            Emitter light;
            light.position = {30.f + i * 40.f, 100.f + i * 20.f};
            light.angle = -150.f + i * 15.f;
            light.colorIndex = static_cast<int>(i % 7);
            light.rayCount = static_cast<int>(i + 1);
            light.bounces = static_cast<int>(i % 13);
            light.spread = i * 3.f;
            light.sweep = i % 2;
            light.mode = i % 2 ? EmitterMode::Point : EmitterMode::Beam;
            many.lasers.push_back(light);
        }
        check(saveScene(many, savePath) && loadScene(loaded, savePath) && sceneData(loaded) == sceneData(many),
              "version 4 round-trips sixteen independently configured lasers");
        many.lasers.clear();
        check(saveScene(many, savePath) && loadScene(loaded, savePath) && loaded.lasers.empty(),
              "version 4 supports saved scenes with no lasers");
        {
            std::ofstream corrupt(savePath);
            corrupt << "LIGHT_LAB_2D 4\n17\n";
        }
        check(!loadScene(loaded, savePath) && loaded.lasers.empty(),
              "excessive laser counts are rejected without replacing a scene");
        {
            std::ofstream legacy(savePath);
            legacy << "LIGHT_LAB_2D 3\n130 250 -15 0 12 4 5 0\n2 1 0 1 0\n0\n1 0\n";
        }
        check(loadScene(loaded, savePath) && loaded.lasers.size() == 1 && loaded.lasers[0].sweep &&
                  loaded.lasers[0].colorIndex == 5 && loaded.lasers[0].angle == -15,
              "version 3 scenes migrate the original laser and its sweep setting");
        {
            std::ofstream legacy(savePath);
            legacy << "LIGHT_LAB_2D 2\n110 205 0 0 1 6 5 0\n3 1 0 0 0\n1\n420 205 112 57\n";
        }
        check(loadScene(loaded, savePath) && loaded.prisms.size() == 1 && loaded.prisms[0].apex == 60 &&
                  loaded.prisms[0].glass == Glass::Flint,
              "version 2 prisms keep their original shape and material");
        {
            std::ofstream legacy(savePath);
            legacy << "LIGHT_LAB_2D 1\n170 450 0 12 45 6 0 0\n1 1 0 0 1\n550 530 710 370 0\n";
        }
        check(loadScene(loaded, savePath) && loaded.prisms.empty() && loaded.surfaces.size() == 1,
              "legacy version 1 scenes still load");
        check(saveScene(relay, savePath) && loadScene(loaded, savePath), "restore save test fixture");
        {
            std::ofstream corrupt(savePath);
            corrupt << "LIGHT_LAB_2D 1\n-50 -50 0 12 45 6 0 0\n1 1 0 0 999999\n";
        }
        check(!loadScene(loaded, savePath) && loaded.surfaces.size() == 3, "bad save does not replace scene");
        {
            std::ofstream corrupt(savePath);
            corrupt << "LIGHT_LAB_2D 2\n110 205 0 0 1 6 5 0\n3 1 0 0 0\n1\n420 205 10000 -57\n";
        }
        check(!loadScene(loaded, savePath) && loaded.surfaces.size() == 3,
              "invalid prism geometry does not replace the current scene");
        {
            std::ofstream corrupt(savePath);
            corrupt << "LIGHT_LAB_2D 3\n110 205 0 0 1 6 5 0\n4 1 0 0 1\n450 440 650 240 2 1.5\n0\n1 1\n";
        }
        check(!loadScene(loaded, savePath) && loaded.surfaces.size() == 3,
              "out-of-range splitter ratios are rejected without replacing the scene");
        std::filesystem::remove(savePath);
        std::cout << "PASS: " << checks << " optical and save checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
