#pragma once

#include "scene.hpp"
#include <filesystem>

namespace lightlab {
const char* presetName(Preset preset);

struct SceneContext {
    std::string path, baseline;
};

class SceneLibrary {
public:
    explicit SceneLibrary(std::filesystem::path directory);
    bool restore(Scene& scene);
    bool preserve(const Scene& scene);
    bool saveAs(const Scene& scene, const std::filesystem::path& path);
    bool open(Scene& scene, const std::filesystem::path& path);
    void begin(const Scene& scene);
    bool finish(const Scene& scene);
    bool modified(const Scene& scene) const;
    std::string title(const Scene& scene) const;

    const std::filesystem::path& currentPath() const { return active; }

    std::filesystem::path scenesDirectory() const { return root / "scenes"; }

    std::vector<std::filesystem::path> savedScenes() const;
    SceneContext context() const;
    void restoreContext(const SceneContext& context);

    const std::string& error() const { return lastError; }

private:
    std::filesystem::path root, active;
    std::string baseline, lastError;
    bool writeRecovery(const Scene& scene);
    bool atomicSave(const Scene& scene, const std::filesystem::path& path);
    std::filesystem::path uniquePath(const std::string& name) const;
};
}
