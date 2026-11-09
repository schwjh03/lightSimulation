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

int main() {
    try {
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
