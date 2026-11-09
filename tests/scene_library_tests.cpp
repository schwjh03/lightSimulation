#include "scene_library.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace lightlab;

int main() {
    int checks = 0;
    auto root = std::filesystem::temp_directory_path() /
                ("lightlab-scenes-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto check = [&](bool condition, const char* label) {
        ++checks;
        if (!condition)
            throw std::runtime_error(label);
    };
    try {
        SceneLibrary library(root);
        Scene scene = makeScene(Preset::Room);
        check(!library.restore(scene) && !library.modified(scene), "fresh library starts with an unmodified starter");
        scene.lasers[0].angle = 31;
        check(library.preserve(scene) && library.savedScenes().size() == 1,
              "switch protection saves an unnamed edited scene to its own file");
        auto first = library.currentPath();
        check(first.extension() == ".lightscene", "saved scenes have the scene extension");
        scene = makeScene(Preset::Blank);
        library.begin(scene);
        check(library.currentPath().empty() && library.savedScenes().size() == 1,
              "new starter does not replace an earlier scene file");
        auto named = library.scenesDirectory() / "My prism experiment.lightscene";
        scene = makeScene(Preset::Dispersion);
        check(library.saveAs(scene, named), "save as creates a named file");
        scene.prisms[0].apex = 48;
        check(library.preserve(scene), "switching away updates the active named scene");
        scene = makeScene(Preset::Relay);
        library.begin(scene);
        scene.lasers[0].angle = 20;
        check(library.open(scene, named) && scene.prisms[0].apex == 48 && library.savedScenes().size() == 3,
              "open preserves current unsaved work and loads the selected saved scene");
        Scene earlier;
        check(loadScene(earlier, first.u8string()) && earlier.lasers[0].angle == 31,
              "earlier scene remains independently reopenable after multiple switches");
        auto copy = library.scenesDirectory() / "Independent copy.lightscene";
        check(library.saveAs(scene, copy), "save as creates a separate copy");
        scene.prisms[0].apex = 42;
        check(library.preserve(scene) && loadScene(earlier, named.u8string()) && earlier.prisms[0].apex == 48,
              "editing a saved copy does not overwrite its original");
        check(library.finish(scene), "normal exit preserves the current scene and startup state");
        SceneLibrary reopened(root);
        Scene resumed;
        check(reopened.restore(resumed) && resumed.prisms[0].apex == 42 && reopened.currentPath() == copy,
              "restarting restores the last active named scene");
        auto context = reopened.context();
        resumed = makeScene(Preset::Blank);
        reopened.begin(resumed);
        reopened.restoreContext(context);
        check(reopened.currentPath() == copy, "undo context restores the correct save destination");
        Scene bad = makeScene(Preset::Room);
        const auto count = reopened.savedScenes().size();
        auto corrupt = root / "broken.lightscene";
        {
            std::ofstream file(corrupt);
            file << "broken";
        }
        check(!reopened.open(bad, corrupt) && bad.surfaces.size() == 4 && reopened.savedScenes().size() == count,
              "invalid open leaves current work and scene library intact");
        auto blocked = root / "directory.lightscene";
        std::filesystem::create_directory(blocked);
        check(!reopened.saveAs(bad, blocked) && reopened.currentPath() == copy,
              "failed save does not change the active save destination");
        auto legacyRoot = root / "legacy";
        std::filesystem::create_directory(legacyRoot);
        auto legacy = makeScene(Preset::SplitterLab);
        check(saveScene(legacy, (legacyRoot / "scene.txt").u8string()), "create legacy recovery fixture");
        SceneLibrary migration(legacyRoot);
        Scene recovered;
        check(migration.restore(recovered) && migration.savedScenes().size() == 1 && recovered.surfaces.size() == 3,
              "legacy single-file save is migrated to a separate recovered scene");
        auto recoveredPath = migration.currentPath();
        recovered = makeScene(Preset::Blank);
        migration.begin(recovered);
        migration.finish(recovered);
        check(loadScene(earlier, recoveredPath.u8string()) && earlier.surfaces.size() == 3,
              "new scenes cannot overwrite the migrated legacy scene");
        auto unicode = library.scenesDirectory() / std::filesystem::u8path("Prism \xc3\xa9tude.lightscene");
        Scene multi = makeScene(Preset::Room);
        multi.lasers.push_back(Emitter{});
        multi.lasers[1].colorIndex = 4;
        multi.lasers[1].angle = 45;
        multi.lasers[1].sweep = true;
        auto multiPath = library.scenesDirectory() / "Two lasers.lightscene";
        check(library.saveAs(multi, multiPath), "named saves accept multiple lasers");
        multi.lasers[1].angle = 80;
        check(library.preserve(multi) && loadScene(earlier, multiPath.u8string()) && earlier.lasers.size() == 2 &&
                  earlier.lasers[1].angle == 80 && earlier.lasers[1].colorIndex == 4 && earlier.lasers[1].sweep,
              "auto-save preserves every laser's own settings");
        check(library.saveAs(legacy, unicode) && loadScene(earlier, unicode.u8string()),
              "named scene files support Unicode names");
        check(!library.saveAs(legacy, root / "scene.txt") && library.currentPath() == unicode,
              "named saves cannot replace the reserved recovery file");
        auto oldRecovery = root / "scene.txt";
        check(library.open(earlier, oldRecovery) && library.currentPath() != oldRecovery,
              "opening the recovery file imports an independent named scene");
        auto imported = library.currentPath();
        scene = makeScene(Preset::Blank);
        library.begin(scene);
        check(loadScene(earlier, imported.u8string()) && earlier.surfaces.size() == 3,
              "switching scenes cannot overwrite an imported recovery scene");
        std::filesystem::remove_all(root);
        std::cout << "PASS: " << checks << " named scene library checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
        std::cerr << "FAIL scene library: " << error.what() << '\n';
        return 1;
    }
}
