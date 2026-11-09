#include "ui_checks.hpp"
#include "raymath.h"
#include <cmath>
#include <set>

namespace lightlab {
UiChecks::UiChecks(App& target, bool test) : app(target), enabled(test) {
    if (!enabled) {
        return;
    }
    auto click = [](float x, float y) { return Pointer{{x, y}, true, true, false}; };
    auto move = [](float x, float y) { return Pointer{{x, y}, false, true, false}; };
    auto release = [](float x, float y) { return Pointer{{x, y}, false, false, true}; };
    steps = {
        {"select mirror tool", click(452, 37), [this] { return app.tool == Tool::Mirror; }},
        {"begin mirror", click(710, 300), [] { return true; }},
        {"extend mirror", move(750, 450), [] { return true; }},
        {"finish mirror", release(750, 450), [this] { return app.scene.surfaces.size() == 5 && app.selected == 4; }},
        {"delete surface", click(237, 740), [this] { return app.scene.surfaces.size() == 4; }},
        {"undo deletion", click(1132, 35), [this] { return app.scene.surfaces.size() == 5; }},
        {"select tool", click(350, 37), [this] { return app.tool == Tool::Select; }},
        {"grab source", click(560, 412), [this] { return app.selected == -2; }},
        {"move source", move(600, 452),
         [this] { return Vector2Distance(app.scene.lasers[0].position, {320, 380}) < .1f; }},
        {"release source", release(600, 452), [] { return true; }},
        {"show normals", click(34, 663), [this] { return app.scene.normals; }},
        {"turn off grid", click(1175, 35), [this] { return !app.scene.grid; }},
        {"relay preset", click(140, 137),
         [this] { return app.scene.preset == Preset::Relay && app.scene.surfaces.size() == 3; }},
        {"blank preset", click(217, 137), [this] { return app.scene.surfaces.empty(); }},
        {"wall tool", click(565, 37), [this] { return app.tool == Tool::Wall; }},
        {"begin wall", click(1000, 350), [] { return true; }},
        {"extend wall", move(1000, 550), [] { return true; }},
        {"finish wall", release(1000, 550),
         [this] { return app.scene.surfaces.size() == 1 && app.scene.surfaces[0].material == Material::Absorber; }},
        {"restore room", click(59, 137),
         [this] { return app.scene.preset == Preset::Room && app.scene.surfaces.size() == 4; }},
        {"grab mirror endpoint", click(1100, 222), [this] { return app.selected == 0; }},
        {"rotate mirror endpoint", move(1060, 242),
         [this] { return Vector2Distance(app.scene.surfaces[0].start, {780, 170}) < .1f; }},
        {"release endpoint", release(1060, 242), [] { return true; }},
        {"grab mirror body", click(1080, 422), [this] { return app.selected == 0; }},
        {"translate mirror", move(1040, 402),
         [this] {
             return Vector2Distance(app.scene.surfaces[0].start, {740, 150}) < .1f &&
                    Vector2Distance(app.scene.surfaces[0].end, {780, 510}) < .1f;
         }},
        {"release mirror", release(1040, 402), [] { return true; }},
        {"select laser before aiming", click(560, 412), [this] { return app.selected == -2; }},
        {"release selected laser", release(560, 412), [] { return true; }},
        {"grab beam handle", click(605, 404), [this] { return app.selected == -2; }},
        {"rotate beam", move(560, 366), [this] { return std::abs(app.scene.lasers[0].angle + 90) < .1f; }},
        {"release beam", release(560, 366), [] { return true; }},
        {"final room reset", click(59, 137),
         [this] { return app.scene.preset == Preset::Room && app.scene.surfaces.size() == 4; }},
        {"prism tool", click(682, 37), [this] { return app.tool == Tool::Prism; }},
        {"begin prism", click(880, 322), [] { return true; }},
        {"place prism", release(880, 322),
         [this] { return app.scene.prisms.size() == 1 && app.selected == -3 && app.tool == Tool::Select; }},
        {"grab prism body", click(880, 322), [this] { return app.selectedPrism == 0; }},
        {"move prism body", move(920, 362),
         [this] { return Vector2Distance(app.scene.prisms[0].center, {640, 290}) < .1f; }},
        {"release prism body", release(920, 362), [] { return true; }},
        {"grab prism corner", click(977, 450), [this] { return app.selected == -3; }},
        {"resize and rotate prism", move(1010, 464),
         [this] { return app.scene.prisms[0].radius > 130 && app.scene.prisms[0].angle < 50; }},
        {"release prism corner", release(1010, 464), [] { return true; }},
        {"delete prism", click(237, 740), [this] { return app.scene.prisms.empty(); }},
        {"undo prism deletion", click(1132, 35), [this] { return app.scene.prisms.size() == 1; }},
        {"prism demo", click(80, 169),
         [this] {
             std::set<int> bands;
             for (const auto& ray : app.rays)
                 if (ray.dispersed)
                     bands.insert(static_cast<int>(ray.wavelength));
             return app.scene.preset == Preset::Dispersion && app.scene.prisms.size() == 1 && bands.size() == 15;
         }},
        {"grab laser housing", click(350, 277),
         [this] { return app.selected == -2 && Vector2Distance(app.scene.lasers[0].position, {110, 205}) < .1f; }},
        {"move laser housing without jumping", move(370, 297),
         [this] { return Vector2Distance(app.scene.lasers[0].position, {130, 225}) < .1f; }},
        {"release laser housing", release(370, 297), [] { return true; }},
        {"restore prism demo", click(80, 169), [this] { return app.scene.lasers[0].colorIndex == 5; }},
        {"detector tool", click(835, 37), [this] { return app.tool == Tool::Detector; }},
        {"begin detector", click(1180, 580), [] { return true; }},
        {"draw detector", move(1180, 720), [] { return true; }},
        {"finish detector", release(1180, 720),
         [this] {
             return app.scene.surfaces.size() == 2 && app.scene.surfaces[1].material == Material::Detector &&
                    app.tool == Tool::Select;
         }},
        {"grab detector body", click(1180, 650), [this] { return app.selected == 1; }},
        {"move detector body", move(1160, 650), [this] { return std::abs(app.scene.surfaces[1].start.x - 880) < .1f; }},
        {"release detector body", release(1160, 650), [] { return true; }},
        {"grab detector endpoint", click(1160, 720), [] { return true; }},
        {"resize detector", move(1120, 720), [this] { return std::abs(app.scene.surfaces[1].end.x - 840) < .1f; }},
        {"release detector endpoint", release(1120, 720), [] { return true; }},
        {"delete detector", click(237, 740), [this] { return app.scene.surfaces.size() == 1; }},
        {"undo detector deletion", click(1132, 35), [this] { return app.scene.surfaces.size() == 2; }},
        {"select prism properties", click(700, 277), [this] { return app.selected == -3; }},
        {"release prism properties", release(700, 277), [] { return true; }},
        {"splitter lab", click(200, 169),
         [this] { return app.scene.preset == Preset::SplitterLab && app.readings.size() == 3; }},
        {"select splitter", click(830, 412), [this] { return app.selected == 0; }},
        {"release splitter", release(830, 412), [] { return true; }},
        {"select detector spectrum", click(830, 192),
         [this] { return app.selected == 1 && app.readings[1].spectrum.size() == 15; }},
        {"release detector spectrum", release(830, 192), [] { return true; }},
        {"detector inspector leaves canvas unobstructed", click(1120, 400), [this] { return app.selected == -1; }},
        {"grid hides animated dots", click(1175, 35),
         [this] {
             pausedBackground = app.backgroundTime;
             return !app.scene.grid;
         }},
        {"hidden grid pauses background", release(1175, 35), [this] { return app.backgroundTime == pausedBackground; }},
        {"grid restores animated dots", click(1175, 35), [this] { return app.scene.grid; }},
        {"visible grid always animates", release(1175, 35), [this] { return app.backgroundTime > pausedBackground; }},
        {"splitter drawing tool", click(736, 37), [this] { return app.tool == Tool::Splitter; }},
        {"begin new splitter", click(580, 622), [] { return true; }},
        {"draw new splitter", move(630, 672), [] { return true; }},
        {"finish new splitter", release(630, 672),
         [this] {
             return app.scene.surfaces.size() == 4 && app.scene.surfaces[3].material == Material::Splitter &&
                    app.tool == Tool::Select;
         }},
        {"reset splitter lab", click(200, 169), [this] { return app.scene.surfaces.size() == 3; }},
        {"open saved scenes action", click(927, 37), [this] { return app.openRequested; }},
        {"save scene action", click(990, 37), [this] { return app.saveRequested; }},
        {"save as action", click(1065, 37), [this] { return app.saveAsRequested; }},
        {"prism demo is grouped with starters", click(80, 169),
         [this] { return app.scene.preset == Preset::Dispersion; }},
        {"add laser tool", click(140, 250), [this] { return app.tool == Tool::Laser; }},
        {"begin second laser", click(450, 520), [] { return true; }},
        {"place second laser", release(450, 520),
         [this] {
             return app.scene.lasers.size() == 2 && app.selectedLaser == 1 && app.scene.lasers[0].colorIndex == 5 &&
                    app.scene.lasers[1].colorIndex == 1;
         }},
        {"add third laser tool", click(140, 250), [this] { return app.tool == Tool::Laser; }},
        {"begin aimed third laser", click(530, 560), [] { return true; }},
        {"aim third laser while placing", move(600, 530), [] { return true; }},
        {"finish third laser", release(600, 530),
         [this] { return app.scene.lasers.size() == 3 && app.selectedLaser == 2 && app.scene.lasers[2].angle < -20; }},
        {"choose laser from sidebar", click(140, 414), [this] { return app.selected == -2 && app.selectedLaser == 1; }},
        {"grab second laser", click(450, 520), [this] { return app.selectedLaser == 1; }},
        {"move only second laser", move(490, 550),
         [this] {
             return Vector2Distance(app.scene.lasers[1].position, {210, 478}) < .1f &&
                    Vector2Distance(app.scene.lasers[0].position, {110, 205}) < .1f;
         }},
        {"release second laser", release(490, 550), [] { return true; }},
        {"grab selected laser aim handle", click(536, 550), [this] { return app.selectedLaser == 1; }},
        {"aim only selected laser", move(490, 450),
         [this] {
             return std::abs(app.scene.lasers[1].angle + 90) < .1f && app.scene.lasers[0].angle == 0 &&
                    app.scene.lasers[2].angle < -20;
         }},
        {"release selected aim handle", release(490, 450), [] { return true; }},
        {"delete one laser", click(237, 740), [this] { return app.scene.lasers.size() == 2; }},
        {"undo laser deletion", click(1132, 35), [this] { return app.scene.lasers.size() == 3; }},
        {"select first laser for removal", click(140, 374), [this] { return app.selectedLaser == 0; }},
        {"remove first laser", click(237, 740), [this] { return app.scene.lasers.size() == 2; }},
        {"select next laser for removal", click(140, 374), [] { return true; }},
        {"remove next laser", click(237, 740), [this] { return app.scene.lasers.size() == 1; }},
        {"select last laser for removal", click(140, 374), [] { return true; }},
        {"remove last laser", click(237, 740), [this] { return app.scene.lasers.empty() && app.rays.empty(); }},
        {"add into empty scene", click(140, 250), [this] { return app.tool == Tool::Laser; }},
        {"begin replacement laser", click(500, 500), [] { return true; }},
        {"finish replacement laser", release(500, 500),
         [this] { return app.scene.lasers.size() == 1 && app.selectedLaser == 0; }}};
    report.open("ui-checks.txt");
}

void UiChecks::beforeFrame() {
    if (enabled && current < steps.size()) {
        app.ui.pointer = steps[current].pointer;
    }
}

void UiChecks::afterFrame() {
    if (!enabled || current >= steps.size()) {
        return;
    }
    const auto& step = steps[current++];
    bool ok = step.verify();
    report << (ok ? "PASS " : "FAIL ") << step.name << '\n';
    if (!ok) {
        code = 2;
        TakeScreenshot("ui-failure.png");
    }
}

bool UiChecks::done() const {
    return enabled && (code != 0 || current == steps.size());
}

int UiChecks::result() const {
    return code;
}
} // namespace lightlab
