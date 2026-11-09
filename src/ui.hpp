#pragma once

#include "raylib.h"
#include <string>

namespace lightlab {
inline constexpr Color paper = {18, 26, 39, 255};
inline constexpr Color ink = {228, 236, 247, 255};
inline constexpr Color muted = {140, 157, 181, 255};
inline constexpr Color border = {43, 58, 78, 255};
inline constexpr Color teal = {25, 144, 162, 255};
inline constexpr Color mirrorColor = {131, 218, 244, 255};
inline constexpr Color wallColor = {239, 137, 118, 255};
inline constexpr Color prismColor = {142, 174, 255, 255};
inline constexpr Color splitterColor = {218, 156, 255, 255};
inline constexpr Color detectorColor = {88, 232, 199, 255};
inline constexpr Color lightColors[] = {{255, 215, 50, 255}, {255, 48, 69, 255},   {63, 255, 121, 255},
                                        {37, 233, 255, 255}, {200, 102, 255, 255}, {246, 250, 255, 255},
                                        {60, 125, 255, 255}};
inline constexpr const char* colorNames[] = {"Yellow / 589 nm",     "Red / 650 nm",          "Green / 532 nm",
                                             "Cyan / blue + green", "Purple / violet + red", "White / full spectrum",
                                             "Blue / 460 nm"};

struct Pointer {
    Vector2 position{};
    bool pressed = false;
    bool down = false;
    bool released = false;
};

class Ui {
public:
    Pointer pointer;
    bool pointerHand = false;
    std::string tooltip;
    void load();
    void unload();
    void begin();
    void finish();
    void text(const std::string& label, float x, float y, float size, Color color = ink, bool heavy = false);
    void center(const std::string& label, float x, float y, float size, Color color = ink, bool heavy = false);
    bool button(Rectangle rect, const std::string& label, bool selected = false, bool enabled = true);
    bool toggle(Rectangle rect, const std::string& label, bool value);
    bool slider(const std::string& id, Rectangle rect, float& value, float minimum, float maximum, bool enabled = true);
    bool hovered(Rectangle rect) const;
    void hint(Rectangle rect, const std::string& label);

private:
    Font regular{}, bold{};
    std::string activeSlider;
};
} // namespace lightlab
