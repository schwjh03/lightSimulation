#ifdef _WIN32
#include "native_windows.hpp"
#endif
#include "scene_library.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace lightlab {
namespace {
bool replaceFile(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    std::error_code error;
    std::filesystem::rename(from, to, error);
    return !error;
#endif
}

bool sameFile(const std::filesystem::path& a, const std::filesystem::path& b) {
    if (a.empty() || b.empty())
        return false;
    std::error_code error;
    bool equal = std::filesystem::equivalent(a, b, error);
    return !error ? equal
                  : std::filesystem::absolute(a).lexically_normal() == std::filesystem::absolute(b).lexically_normal();
}
}

const char* presetName(Preset preset) {
    switch (preset) {
    case Preset::Room:
        return "Room";
    case Preset::Relay:
        return "Relay";
    case Preset::Blank:
        return "Blank";
    case Preset::Dispersion:
        return "Prism Demo";
    default:
        return "Splitter Lab";
    }
}

SceneLibrary::SceneLibrary(std::filesystem::path directory) : root(std::move(directory)) {
    std::error_code error;
    std::filesystem::create_directories(scenesDirectory(), error);
    if (error)
        lastError = "Could not create the scenes folder";
}

std::filesystem::path SceneLibrary::uniquePath(const std::string& name) const {
    auto candidate = scenesDirectory() / std::filesystem::u8path(name + ".lightscene");
    for (int number = 2; std::filesystem::exists(candidate); ++number)
        candidate = scenesDirectory() / std::filesystem::u8path(name + " (" + std::to_string(number) + ").lightscene");
    return candidate;
}

bool SceneLibrary::atomicSave(const Scene& scene, const std::filesystem::path& path) {
    auto temporary = path;
    temporary += ".tmp";
    if (!saveScene(scene, temporary.u8string())) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        lastError = "Could not write scene file";
        return false;
    }
    if (!replaceFile(temporary, path)) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        lastError = "Could not replace scene file; the previous file was kept";
        return false;
    }
    lastError.clear();
    return true;
}

bool SceneLibrary::writeRecovery(const Scene& scene) {
    if (!atomicSave(scene, root / "scene.txt"))
        return false;
    auto temporary = root / "session.txt.tmp";
    std::ofstream state(temporary);
    state << std::quoted(active.u8string()) << '\n';
    state.close();
    if (!state || !replaceFile(temporary, root / "session.txt")) {
        lastError = "Scene was saved, but the startup session could not be updated";
        return false;
    }
    return true;
}

bool SceneLibrary::restore(Scene& scene) {
    std::string previous;
    const bool hasSession = std::filesystem::exists(root / "session.txt");
    std::ifstream state(root / "session.txt");
    state >> std::quoted(previous);
    if (!previous.empty() && loadScene(scene, previous)) {
        if (sameFile(std::filesystem::u8path(previous), root / "scene.txt"))
            return saveAs(scene, uniquePath("Recovered scene"));
        active = std::filesystem::u8path(previous);
        baseline = sceneData(scene);
        return true;
    }
    if (loadScene(scene, (root / "scene.txt").u8string())) {
        baseline = sceneData(scene);
        // Preserve the old single-file save as an independently reopenable scene.
        if (!hasSession || !previous.empty())
            return saveAs(scene, uniquePath("Recovered scene"));
        return true;
    }
    baseline = sceneData(scene);
    return false;
}

bool SceneLibrary::modified(const Scene& scene) const {
    return sceneData(scene) != baseline;
}

bool SceneLibrary::saveAs(const Scene& scene, const std::filesystem::path& path) {
    std::error_code error;
    auto destination = std::filesystem::absolute(path, error);
    for (const auto* reserved : {"scene.txt", "scene.txt.tmp", "session.txt", "session.txt.tmp"}) {
        if (sameFile(destination, root / reserved)) {
            lastError = "Choose a separate scene file; that file is reserved for session recovery";
            return false;
        }
    }
    if (error || !atomicSave(scene, destination))
        return false;
    active = destination;
    baseline = sceneData(scene);
    return writeRecovery(scene);
}

bool SceneLibrary::preserve(const Scene& scene) {
    if (active.empty() && !modified(scene))
        return true;
    auto destination = active;
    if (destination.empty()) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif
        std::ostringstream name;
        name << presetName(scene.preset) << " - " << std::put_time(&local, "%Y-%m-%d %H-%M-%S");
        destination = uniquePath(name.str());
    }
    return saveAs(scene, destination);
}

bool SceneLibrary::open(Scene& scene, const std::filesystem::path& path) {
    Scene loaded;
    if (!loadScene(loaded, path.u8string())) {
        lastError = "That file is not a valid Light Lab scene";
        return false;
    }
    if (!preserve(scene))
        return false;
    // Reload only when opening the current named file, just updated by preserve().
    if (sameFile(path, active) && !loadScene(loaded, path.u8string())) {
        lastError = "Could not reopen scene";
        return false;
    }
    scene = std::move(loaded);
    if (sameFile(path, root / "scene.txt"))
        return saveAs(scene, uniquePath("Imported recovery scene"));
    active = std::filesystem::absolute(path);
    baseline = sceneData(scene);
    return writeRecovery(scene);
}

void SceneLibrary::begin(const Scene& scene) {
    active.clear();
    baseline = sceneData(scene);
    writeRecovery(scene);
}

bool SceneLibrary::finish(const Scene& scene) {
    return preserve(scene) && writeRecovery(scene);
}

std::string SceneLibrary::title(const Scene& scene) const {
    return active.empty() ? std::string("Untitled ") + presetName(scene.preset) : active.stem().u8string();
}

std::vector<std::filesystem::path> SceneLibrary::savedScenes() const {
    std::vector<std::filesystem::path> paths;
    std::error_code error;
    for (std::filesystem::directory_iterator i(scenesDirectory(), error), end; !error && i != end; i.increment(error)) {
        if (i->is_regular_file() && i->path().extension() == ".lightscene")
            paths.push_back(i->path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

SceneContext SceneLibrary::context() const {
    return {active.u8string(), baseline};
}

void SceneLibrary::restoreContext(const SceneContext& context) {
    active = std::filesystem::u8path(context.path);
    baseline = context.baseline;
    Scene onDisk;
    if (!active.empty() && loadScene(onDisk, active.u8string()))
        baseline = sceneData(onDisk);
}
}
