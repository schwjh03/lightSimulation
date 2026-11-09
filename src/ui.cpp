#include "ui.hpp"
#include <algorithm>
#include <cstdlib>

namespace lightlab {
void Ui::load() {
    const char* windows = std::getenv("WINDIR");
    std::string directory = std::string(windows ? windows : "C:/Windows") + "/Fonts/";
    regular = LoadFontEx((directory + "segoeui.ttf").c_str(), 48, nullptr, 0);
    bold = LoadFontEx((directory + "segoeuib.ttf").c_str(), 56, nullptr, 0);
    if (!regular.texture.id) {
        regular = GetFontDefault();
    }
    if (!bold.texture.id) {
        bold = GetFontDefault();
    }
    SetTextureFilter(regular.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(bold.texture, TEXTURE_FILTER_BILINEAR);
}

void Ui::unload() {
    if (regular.texture.id != GetFontDefault().texture.id) {
        UnloadFont(regular);
    }
    if (bold.texture.id != GetFontDefault().texture.id) {
        UnloadFont(bold);
    }
}

void Ui::begin() {
    tooltip.clear();
    pointerHand = false;
    if (!pointer.down) {
        activeSlider.clear();
    }
}

void Ui::text(const std::string& label, float x, float y, float size, Color color, bool heavy) {
    DrawTextEx(heavy ? bold : regular, label.c_str(), {x, y}, size, 0, color);
}

void Ui::center(const std::string& label, float x, float y, float size, Color color, bool heavy) {
    float width = MeasureTextEx(heavy ? bold : regular, label.c_str(), size, 0).x;
    text(label, x - width / 2, y, size, color, heavy);
}

bool Ui::hovered(Rectangle rect) const {
    return CheckCollisionPointRec(pointer.position, rect);
}

bool Ui::button(Rectangle rect, const std::string& label, bool selected, bool enabled) {
    bool hot = hovered(rect) && enabled;
    Color fill = selected ? teal : hot ? Color{43, 61, 83, 255} : Color{28, 40, 58, 255};
    DrawRectangleRounded(rect, .18f, 8, enabled ? fill : Color{22, 31, 45, 255});
    DrawRectangleRoundedLinesEx(rect, .12f, 8, 1, selected ? teal : border);
    center(label, rect.x + rect.width / 2, rect.y + (rect.height - 19) / 2, 19,
           !enabled   ? muted
           : selected ? WHITE
                      : ink,
           true);
    if (hot) {
        pointerHand = true;
    }
    return hot && pointer.pressed;
}

bool Ui::toggle(Rectangle rect, const std::string& label, bool value) {
    Rectangle mark = {rect.x, rect.y + 3, 18, 18};
    DrawRectangleRounded(mark, .12f, 4, value ? teal : Color{28, 40, 58, 255});
    DrawRectangleRoundedLinesEx(mark, .12f, 4, 1, value ? teal : border);
    if (value) {
        DrawLineEx({mark.x + 4, mark.y + 9}, {mark.x + 8, mark.y + 13}, 2, WHITE);
        DrawLineEx({mark.x + 8, mark.y + 13}, {mark.x + 15, mark.y + 5}, 2, WHITE);
    }
    text(label, rect.x + 29, rect.y, 19);
    if (hovered(rect)) {
        pointerHand = true;
    }
    return hovered(rect) && pointer.pressed;
}

bool Ui::slider(const std::string& id, Rectangle rect, float& value, float minimum, float maximum, bool enabled) {
    if (enabled && hovered(rect) && pointer.pressed) {
        activeSlider = id;
    }
    float old = value;
    if (enabled && activeSlider == id && pointer.down) {
        value = minimum + std::clamp((pointer.position.x - rect.x) / rect.width, 0.f, 1.f) * (maximum - minimum);
    }
    float fraction = std::clamp((value - minimum) / (maximum - minimum), 0.f, 1.f);
    float y = rect.y + rect.height / 2;
    DrawRectangleRounded({rect.x, y - 2, rect.width, 4}, 1, 8, border);
    DrawRectangleRounded({rect.x, y - 2, rect.width * fraction, 4}, 1, 8, enabled ? teal : muted);
    DrawCircleV({rect.x + rect.width * fraction, y}, 6, enabled ? teal : muted);
    DrawCircleV({rect.x + rect.width * fraction, y}, 2, WHITE);
    if (enabled && hovered(rect)) {
        pointerHand = true;
    }
    return old != value;
}

void Ui::hint(Rectangle rect, const std::string& label) {
    if (hovered(rect)) {
        tooltip = label;
    }
}

void Ui::finish() {
    if (!tooltip.empty()) {
        float width = MeasureTextEx(regular, tooltip.c_str(), 17, 0).x + 24;
        float x = std::clamp(pointer.position.x - width / 2, 8.f, 1272 - width);
        float y = std::clamp(pointer.position.y + 25, 8.f, 756.f);
        DrawRectangleRounded({x, y, width, 30}, .1f, 8, Color{35, 41, 45, 250});
        text(tooltip, x + 12, y + 4, 17, WHITE);
    }
    SetMouseCursor(pointerHand ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
}
} // namespace lightlab
