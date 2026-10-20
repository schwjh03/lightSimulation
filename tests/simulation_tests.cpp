#include "editor.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
bool near(sf::Vector2f a, sf::Vector2f b) { return length(a - b) < 0.02f; }
void click(Editor& e, sf::Vector2f p) { e.pointerDown(p); e.pointerUp(p); }
void drag(Editor& e, sf::Vector2f from, sf::Vector2f to)
{
    e.pointerDown(from); e.pointerMove(to); e.pointerUp(to);
}

void opticsTests()
{
    const sf::FloatRect bounds({0.f, 0.f}, {500.f, 500.f});
    auto path = traceBeam({100.f, 200.f}, {1.f, 0.f}, {}, bounds);
    require(path.size() == 1 && near(path[0].b, {500.f, 200.f}), "Empty beam must stop at canvas boundary");
    std::vector<Surface> scene = {{{250.f, 50.f}, {250.f, 450.f}, Material::Wall}};
    path = traceBeam({100.f, 200.f}, {1.f, 0.f}, scene, bounds);
    require(path.size() == 1 && near(path[0].b, {250.f, 200.f}), "Walls must absorb the beam");
    scene[0].material = Material::Mirror;
    path = traceBeam({100.f, 200.f}, {1.f, 0.f}, scene, bounds);
    require(path.size() == 2 && near(path[1].b, {0.f, 200.f}), "Vertical mirror must reflect back horizontally");
    scene = {{{200.f, 100.f}, {400.f, 300.f}, Material::Mirror},
             {{100.f, 350.f}, {450.f, 350.f}, Material::Wall}};
    path = traceBeam({100.f, 200.f}, {1.f, 0.f}, scene, bounds);
    require(path.size() == 2 && near(path[0].b, {300.f, 200.f}) && near(path[1].b, {300.f, 350.f}),
            "Angled mirror must reflect into the nearest wall");
    scene = {{{250.f, 0.f}, {250.f, 500.f}, Material::Mirror},
             {{50.f, 0.f}, {50.f, 500.f}, Material::Mirror}};
    path = traceBeam({100.f, 200.f}, {1.f, 0.f}, scene, bounds, 8);
    require(path.size() == 9, "Opposing mirrors must respect the reflection limit");
    require(traceBeam({100.f, 100.f}, {}, scene, bounds).empty(), "Zero direction must be safe");
    scene = {{{250.f, 200.f}, {250.f, 200.f}, Material::Mirror}};
    path = traceBeam({100.f, 200.f}, {1.f, 0.f}, scene, bounds);
    require(path.size() == 1 && near(path[0].b, {500.f, 200.f}), "Degenerate surfaces must be ignored");
    scene = {{{250.f, 50.f}, {250.f, 450.f}, Material::Mirror},
             {{250.f, 50.f}, {250.f, 450.f}, Material::Wall}};
    require(traceBeam({100.f, 200.f}, {1.f, 0.f}, scene, bounds).size() == 1,
            "Coincident walls must absorb rather than reflect");
    scene = {{{100.f, 250.f}, {400.f, 250.f}, Material::Mirror}};
    path = traceBeam({100.f, 100.f}, {0.f, 1.f}, scene, bounds);
    require(path.size() == 2 && near(path[0].b, {100.f, 250.f}), "A mirror endpoint must count as a hit");
}

void editorTests()
{
    Editor e({{20.f, 80.f}, {860.f, 640.f}});
    e.beginPaletteDrag(Tool::Laser);
    e.pointerUp({120.f, 200.f});
    require(e.getLasers().size() == 1, "Dropping a laser must create a node");
    for (const auto color : {sf::Color::Green, sf::Color::Red, sf::Color::Blue,
                             sf::Color(184, 115, 255), sf::Color::White}) {
        e.setColor(color);
        require(e.getLasers()[0].getColor() == color, "All five colors must apply to a selected laser");
    }
    drag(e, {120.f, 200.f}, {180.f, 240.f});
    require(near(e.getLasers()[0].getPosition(), {180.f, 240.f}), "Laser body must move");
    drag(e, e.getLasers()[0].aimHandle(), {180.f, 400.f});
    require(near(e.getLasers()[0].aimHandle(), {180.f, 286.f}), "Laser handle must rotate the beam");
    e.setTool(Tool::Mirror);
    click(e, {400.f, 200.f});
    e.pointerMove({500.f, 300.f});
    require(e.preview().has_value() && e.getSurfaces().empty(), "First click must create only a preview");
    click(e, {500.f, 300.f});
    require(e.getSurfaces().size() == 1 && !e.preview(), "Second click must finish the mirror");
    drag(e, {500.f, 300.f}, {560.f, 360.f});
    require(near(e.getSurfaces()[0].b, {560.f, 360.f}), "Endpoint must resize and rotate");
    drag(e, {480.f, 280.f}, {510.f, 320.f});
    require(near(e.getSurfaces()[0].a, {430.f, 240.f}) && near(e.getSurfaces()[0].b, {590.f, 400.f}),
            "Dragging the body must translate both endpoints equally");
    e.pointerDown({430.f, 240.f}); e.pointerMove({300.f, 140.f}); e.cancel();
    require(near(e.getSurfaces()[0].a, {430.f, 240.f}), "Cancel must restore the original geometry");
    drag(e, {590.f, 400.f}, {1200.f, 1000.f});
    require(e.contains(e.getSurfaces()[0].b), "Endpoints must stay inside canvas");
    const float originalLength = length(e.getSurfaces()[0].b - e.getSurfaces()[0].a);
    const auto midpoint = (e.getSurfaces()[0].a + e.getSurfaces()[0].b) / 2.f;
    drag(e, midpoint, {-100.f, -100.f});
    require(std::abs(length(e.getSurfaces()[0].b - e.getSurfaces()[0].a) - originalLength) < 0.02f,
            "Body movement at canvas boundaries must preserve length");
    e.setTool(Tool::Wall);
    drag(e, {650.f, 160.f}, {700.f, 260.f});
    require(e.getSurfaces().size() == 2 && e.getSurfaces()[1].material == Material::Wall,
            "Dragging the wall tool must draw an absorbing wall");
    drag(e, {700.f, 260.f}, {750.f, 300.f});
    require(near(e.getSurfaces()[1].b, {750.f, 300.f}), "Wall endpoint must be editable too");
    e.deleteSelected();
    require(e.getSurfaces().size() == 1, "Delete must remove only the selected object");
    e.beginPaletteDrag(Tool::Mirror); e.pointerUp({600.f, 450.f});
    require(e.preview().has_value(), "Dropping a mirror must start its first endpoint");
    click(e, {700.f, 550.f});
    require(e.getSurfaces().size() == 2, "Click must finish a dropped mirror");
    e.setTool(Tool::Wall); click(e, {650.f, 650.f}); e.cancel();
    require(!e.preview() && e.getSurfaces().size() == 2, "Cancel must discard unfinished placement");
    e.beginPaletteDrag(Tool::Laser); e.pointerUp({1000.f, 250.f});
    require(e.getLasers().size() == 1, "Dropping outside the canvas must not create a laser");
    e.setTool(Tool::Wall); click(e, {650.f, 650.f}); click(e, {651.f, 650.f});
    require(e.getSurfaces().size() == 2 && e.preview(), "Too-short surfaces must remain a preview");
}
}

int main()
{
    try {
        opticsTests();
        editorTests();
        std::cout << "PASS: optics and canvas interaction tests\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
