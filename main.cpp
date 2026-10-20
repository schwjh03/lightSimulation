#include "editor.hpp"
#include "values.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
const sf::Color background(12, 17, 25), panel(20, 27, 38), text(229, 237, 247),
                muted(143, 160, 182), accent(101, 184, 255), border(42, 55, 73);
const std::array<sf::Color, 5> colors = {
    sf::Color(72, 237, 147), sf::Color(255, 82, 103), sf::Color(77, 155, 255),
    sf::Color(184, 115, 255), sf::Color::White
};
const std::array<const char*, 5> colorNames = {"Green", "Red", "Blue", "Purple", "White"};
const std::array<Tool, 4> tools = {Tool::Select, Tool::Laser, Tool::Mirror, Tool::Wall};
const std::array<const char*, 4> toolNames = {"Select / move", "Laser node", "Mirror", "Wall"};
const std::array<const char*, 4> toolDetails = {"Edit objects on the canvas", "Drag onto canvas or click to place",
                                               "Reflects light - draw two endpoints", "Blocks light - draw two endpoints"};

sf::FloatRect toolRect(std::size_t i) { return {{920.f, 112.f + i * 66.f}, {260.f, 56.f}}; }
sf::FloatRect colorRect(std::size_t i) { return {{922.f + i * 52.f, 438.f}, {44.f, 40.f}}; }
const sf::FloatRect deleteRect({920.f, 656.f}, {260.f, 40.f});

void box(sf::RenderTarget& target, sf::FloatRect bounds, sf::Color fill,
         sf::Color outline = sf::Color::Transparent, float thickness = 1.f)
{
    sf::RectangleShape shape(bounds.size);
    shape.setPosition(bounds.position);
    shape.setFillColor(fill);
    shape.setOutlineColor(outline);
    shape.setOutlineThickness(thickness);
    target.draw(shape);
}

void label(sf::RenderTarget& target, const sf::Font& font, const std::string& value,
           sf::Vector2f position, unsigned size = 14, sf::Color color = text)
{
    sf::Text item(font, value, size);
    item.setPosition(position);
    item.setFillColor(color);
    target.draw(item);
}

void line(sf::RenderTarget& target, sf::Vector2f a, sf::Vector2f b, sf::Color color, float width)
{
    const auto delta = b - a;
    sf::RectangleShape shape({length(delta), width});
    shape.setOrigin({0.f, width / 2.f});
    shape.setPosition(a);
    shape.setRotation(sf::radians(std::atan2(delta.y, delta.x)));
    shape.setFillColor(color);
    target.draw(shape);
}

void handle(sf::RenderTarget& target, sf::Vector2f point, sf::Color fill)
{
    sf::CircleShape circle(6.f, 24);
    circle.setOrigin({6.f, 6.f});
    circle.setPosition(point);
    circle.setFillColor(fill);
    circle.setOutlineColor(sf::Color::White);
    circle.setOutlineThickness(2.f);
    target.draw(circle);
}

void surface(sf::RenderTarget& target, const Surface& item, bool selected, bool preview = false)
{
    const bool mirror = item.material == Material::Mirror;
    const auto tint = mirror ? sf::Color(136, 214, 255) : sf::Color(225, 162, 103);
    if (selected) line(target, item.a, item.b, sf::Color(52, 87, 119), 12.f);
    line(target, item.a, item.b, preview ? sf::Color(tint.r, tint.g, tint.b, 140) : tint, mirror ? 4.f : 7.f);
    if (mirror) line(target, item.a, item.b, sf::Color(223, 245, 255), 1.f);
    if (selected || preview) { handle(target, item.a, tint); handle(target, item.b, tint); }
}

bool loadFont(sf::Font& font)
{
    std::vector<std::filesystem::path> paths;
    if (const char* custom = std::getenv("LIGHTSIM_FONT")) paths.emplace_back(custom);
    paths.emplace_back("assets/Inter-Regular.ttf");
    if (const char* windows = std::getenv("WINDIR")) {
        paths.emplace_back(std::filesystem::path(windows) / "Fonts/segoeui.ttf");
        paths.emplace_back(std::filesystem::path(windows) / "Fonts/arial.ttf");
    }
    paths.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    paths.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    paths.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
    for (const auto& path : paths) {
        if (std::filesystem::exists(path) && font.openFromFile(path)) return true;
    }
    return false;
}

void drawPanel(sf::RenderTarget& target, const sf::Font& font, const Editor& editor, sf::Vector2f mouse)
{
    box(target, {{PANEL_X, 0.f}, {300.f, 800.f}}, panel);
    line(target, {PANEL_X, 0.f}, {PANEL_X, 800.f}, border, 1.f);
    label(target, font, "WORKBENCH", {920.f, 25.f}, 13, accent);
    label(target, font, "Build a light path", {920.f, 48.f}, 23);
    label(target, font, "TOOLS", {920.f, 88.f}, 11, muted);
    for (std::size_t i = 0; i < tools.size(); ++i) {
        const auto r = toolRect(i);
        const bool active = editor.getTool() == tools[i];
        box(target, r, active ? sf::Color(32, 58, 82) :
            r.contains(mouse) ? sf::Color(33, 44, 59) : sf::Color(26, 35, 48), active ? accent : border);
        label(target, font, std::to_string(i + 1), r.position + sf::Vector2f(12.f, 9.f), 15, active ? accent : muted);
        label(target, font, toolNames[i], r.position + sf::Vector2f(36.f, 6.f), 16);
        label(target, font, toolDetails[i], r.position + sf::Vector2f(36.f, 31.f), 10, muted);
    }
    line(target, {920.f, 397.f}, {1180.f, 397.f}, border, 1.f);
    label(target, font, "LASER COLOR", {920.f, 412.f}, 11, muted);
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const bool chosen = colors[i] == editor.getColor();
        box(target, colorRect(i), sf::Color(29, 40, 55), chosen ? sf::Color::White : border, chosen ? 2.f : 1.f);
        auto r = colorRect(i);
        r.position += sf::Vector2f(7.f, 7.f);
        r.size -= sf::Vector2f(14.f, 14.f);
        box(target, r, colors[i]);
        if (chosen) label(target, font, colorNames[i], {920.f, 490.f}, 14, colors[i]);
    }
    label(target, font, "Applies to selected + new lasers", {920.f, 514.f}, 12, muted);
    line(target, {920.f, 551.f}, {1180.f, 551.f}, border, 1.f);
    label(target, font, "SELECTION", {920.f, 567.f}, 11, muted);
    const auto selected = editor.getSelection();
    std::string name = "Nothing selected";
    std::string detail = "Click any object on the canvas.";
    if (selected.kind == ObjectKind::Laser) {
        name = "Laser node";
        detail = "Move source / drag handle to aim.";
    } else if (selected.kind == ObjectKind::Surface) {
        const auto& s = editor.getSurfaces()[selected.index];
        name = s.material == Material::Mirror ? "Mirror" : "Wall";
        name += "  /  " + std::to_string(static_cast<int>(length(s.b - s.a))) + " px";
        detail = "Drag ends to resize or rotate.";
    }
    label(target, font, name, {920.f, 591.f}, 18);
    label(target, font, detail, {920.f, 623.f}, 12, muted);
    const bool canDelete = selected.kind != ObjectKind::None;
    box(target, deleteRect, canDelete ? sf::Color(65, 37, 46) : sf::Color(26, 32, 43), border);
    label(target, font, "Delete selected", {934.f, 665.f}, 13, canDelete ? sf::Color(255, 155, 166) : muted);
    label(target, font, "Del", {1148.f, 665.f}, 12, muted);
    label(target, font, "1-4  Switch tools     Esc  Cancel", {920.f, 728.f}, 12, muted);
    label(target, font, "Mirrors reflect. Walls absorb.", {920.f, 751.f}, 12, muted);
    label(target, font, "Light updates as you edit.", {920.f, 772.f}, 11, muted);
}
}

int main()
{
    sf::Font font;
    if (!loadFont(font)) {
        std::cerr << "No UI font found. Set LIGHTSIM_FONT to a TrueType font file.\n";
        return 1;
    }
    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}),
                            "Light Simulation", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(120);
    const sf::FloatRect canvas({20.f, 84.f}, {860.f, 644.f});
    Editor editor(canvas);
    sf::Vector2f mouse;
    while (window.isOpen()) {
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            if (event->is<sf::Event::FocusLost>()) editor.cancel();
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                using Key = sf::Keyboard::Key;
                if (key->code == Key::Escape) { editor.cancel(); editor.setTool(Tool::Select); }
                if (key->code == Key::Delete || key->code == Key::Backspace) editor.deleteSelected();
                if (key->code == Key::Num1) editor.setTool(Tool::Select);
                if (key->code == Key::Num2) editor.setTool(Tool::Laser);
                if (key->code == Key::Num3) editor.setTool(Tool::Mirror);
                if (key->code == Key::Num4) editor.setTool(Tool::Wall);
            }
            if (const auto* moved = event->getIf<sf::Event::MouseMoved>()) {
                mouse = window.mapPixelToCoords(moved->position);
                editor.pointerMove(mouse);
            }
            if (const auto* pressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                mouse = window.mapPixelToCoords(pressed->position);
                if (pressed->button == sf::Mouse::Button::Right) { editor.cancel(); editor.setTool(Tool::Select); }
                if (pressed->button == sf::Mouse::Button::Left) {
                    if (mouse.x >= PANEL_X) {
                        for (std::size_t i = 0; i < tools.size(); ++i) {
                            if (toolRect(i).contains(mouse)) editor.beginPaletteDrag(tools[i]);
                        }
                        for (std::size_t i = 0; i < colors.size(); ++i) {
                            if (colorRect(i).contains(mouse)) editor.setColor(colors[i]);
                        }
                        if (deleteRect.contains(mouse)) editor.deleteSelected();
                    } else editor.pointerDown(mouse);
                }
            }
            if (const auto* released = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (released->button == sf::Mouse::Button::Left) {
                    mouse = window.mapPixelToCoords(released->position);
                    editor.pointerUp(mouse);
                }
            }
        }
        if (!window.isOpen()) break;
        window.clear(background);
        label(window, font, "LIGHT SIMULATION", {24.f, 16.f}, 24);
        label(window, font, "An optical playground", {25.f, 49.f}, 13, muted);
        label(window, font, std::to_string(editor.getLasers().size()) + " lasers  /  " +
              std::to_string(editor.getSurfaces().size()) + " surfaces", {656.f, 34.f}, 13, muted);
        box(window, canvas, sf::Color(14, 21, 31), border);
        for (float x = 40.f; x < 880.f; x += 24.f) {
            for (float y = 104.f; y < 728.f; y += 24.f) {
                box(window, {{x, y}, {1.f, 1.f}}, sf::Color(38, 50, 67));
            }
        }
        if (editor.getLasers().empty() && editor.getSurfaces().empty() && !editor.preview()) {
            label(window, font, "Start with a laser", {342.f, 340.f}, 24, sf::Color(127, 151, 179));
            label(window, font, "Drag a node from the workbench onto this canvas.", {289.f, 379.f}, 14, muted);
            label(window, font, "Add mirrors and walls to shape its path.", {317.f, 405.f}, 14, muted);
        }
        // Clip all canvas content, including handles and beam glow, to its bounds.
        const auto originalView = window.getView();
        sf::View canvasView(canvas);
        canvasView.setViewport({{canvas.position.x / WINDOW_WIDTH, canvas.position.y / WINDOW_HEIGHT},
                                {canvas.size.x / WINDOW_WIDTH, canvas.size.y / WINDOW_HEIGHT}});
        window.setView(canvasView);
        const auto selected = editor.getSelection();
        for (const auto& laser : editor.getLasers()) laser.drawBeam(window, editor.getSurfaces(), canvas);
        for (std::size_t i = 0; i < editor.getSurfaces().size(); ++i) {
            surface(window, editor.getSurfaces()[i], selected.kind == ObjectKind::Surface && selected.index == i);
        }
        for (std::size_t i = 0; i < editor.getLasers().size(); ++i) {
            editor.getLasers()[i].drawNode(window, selected.kind == ObjectKind::Laser && selected.index == i);
        }
        if (const auto preview = editor.preview()) surface(window, *preview, false, true);
        if (editor.isPaletteDragging() && editor.contains(mouse) && editor.getTool() == Tool::Laser) {
            handle(window, mouse, editor.getColor());
        }
        window.setView(originalView);
        label(window, font, editor.hint(), {24.f, 750.f}, 14, text);
        label(window, font, "Select an object to reveal its editing handles.", {24.f, 776.f}, 11, muted);
        drawPanel(window, font, editor, mouse);
        window.display();
    }
}
