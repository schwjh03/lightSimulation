#pragma once

#include "optics.hpp"
#include "ui.hpp"
#include "scene_library.hpp"
#include <memory>

namespace lightlab {
enum class Tool { Select, Mirror, Wall, Prism, Splitter, Detector, Laser };
enum class Drag {
    None,
    Source,
    Direction,
    FirstEnd,
    SecondEnd,
    Surface,
    Create,
    PrismMove,
    PrismCorner,
    CreatePrism,
    CreateLaser
};

class App {
public:
    explicit App(bool testing);
    void update(float deltaTime);
    void render();
    void save();
    void saveSession();
    bool saveNamed(const std::string& path);
    bool openNamed(const std::string& path);
    void requestSelectionWindow();
    void checkpoint();
    void undo();
    void eraseSelection();
    void choosePreset(Preset preset);
    void ensureSurfaceIds();
    void requestDetectorWindow(int surfaceIndex);

    Scene scene = makeScene(Preset::Room);
    Ui ui;
    Tool tool = Tool::Select;
    int selected = -2;
    int selectedPrism = -1;
    int selectedLaser = 0;
    std::vector<RaySegment> rays;
    std::vector<DetectorReading> readings;
    double backgroundTime = 0;
    std::vector<std::uint64_t> detectorWindowRequests;
    std::unique_ptr<SceneLibrary> library;
    bool saveRequested = false, saveAsRequested = false, openRequested = false;

private:
    bool testing;
    std::string status = "Ready";
    float statusTime = 0;
    Drag drag = Drag::None;
    Vector2 anchor{};
    Vector2 sourceGrabOffset{};
    Vector2 draftEnd{};
    Surface original{};
    Prism originalPrism{};
    int prismCorner = 0;
    int laserListOffset = 0;
    double lastObjectClickTime = -1;
    std::uint64_t lastObjectClickId = 0;
    Vector2 lastObjectClickPosition{};
    bool pointerMoved = false;

    struct HistoryEntry {
        Scene scene;
        SceneContext context;
    };

    std::vector<HistoryEntry> history;
    std::uint64_t nextSurfaceId = 1;

    void handleCanvas();
    void drawToolbar();
    void drawInspector();
    void drawCanvas();
    void drawStatus();
};

int runWindow(int argc, char** argv);
} // namespace lightlab
