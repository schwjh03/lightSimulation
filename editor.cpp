#include "editor.hpp"
#include "values.hpp"
#include <algorithm>

sf::Vector2f Editor::clamp(sf::Vector2f point) const
{
    return {std::clamp(point.x, bounds.position.x + 2.f, bounds.position.x + bounds.size.x - 2.f),
            std::clamp(point.y, bounds.position.y + 2.f, bounds.position.y + bounds.size.y - 2.f)};
}

void Editor::cancel()
{
    // Escape/focus loss cancels an in-progress edit as well as placement.
    if (drag == Drag::Body || drag == Drag::A || drag == Drag::B) {
        surfaces[selected.index].a = originalA;
        surfaces[selected.index].b = originalB;
    } else if (drag == Drag::Source) {
        lasers[selected.index].setPosition(originalA);
    } else if (drag == Drag::Aim) {
        lasers[selected.index].setAimPoint(originalA);
    }
    anchor.reset();
    drag = Drag::None;
    paletteDrag = drawingPress = finishingClick = false;
}

void Editor::setTool(Tool next) { cancel(); tool = next; }

void Editor::setColor(sf::Color next)
{
    color = next;
    if (selected.kind == ObjectKind::Laser) lasers[selected.index].setColor(next);
}

void Editor::beginPaletteDrag(Tool next) { setTool(next); paletteDrag = true; }

void Editor::addLaser(sf::Vector2f point)
{
    point = clamp(point);
    lasers.emplace_back(point, point + sf::Vector2f(100.f, 0.f), color);
    selected = {ObjectKind::Laser, lasers.size() - 1};
    tool = Tool::Select;
}

bool Editor::selectAt(sf::Vector2f point)
{
    // Handles get priority so a selected object remains easy to reshape.
    if (selected.kind == ObjectKind::Surface) {
        const auto& s = surfaces[selected.index];
        if (length(point - s.a) <= 11.f) drag = Drag::A;
        else if (length(point - s.b) <= 11.f) drag = Drag::B;
        if (drag != Drag::None) { originalA = s.a; originalB = s.b; return true; }
    } else if (selected.kind == ObjectKind::Laser &&
               length(point - lasers[selected.index].aimHandle()) <= 10.f) {
        drag = Drag::Aim;
        originalA = lasers[selected.index].aimHandle();
        return true;
    }
    for (std::size_t i = lasers.size(); i-- > 0;) {
        if (length(point - lasers[i].getPosition()) <= 15.f) {
            selected = {ObjectKind::Laser, i};
            color = lasers[i].getColor();
            drag = Drag::Source;
            originalA = lasers[i].getPosition();
            grabOffset = point - originalA;
            return true;
        }
    }
    for (std::size_t i = surfaces.size(); i-- > 0;) {
        const auto& s = surfaces[i];
        if (distanceToSegment(point, s.a, s.b) <= 10.f) {
            selected = {ObjectKind::Surface, i};
            originalA = s.a; originalB = s.b; pressPoint = point;
            drag = length(point - s.a) <= 11.f ? Drag::A :
                   length(point - s.b) <= 11.f ? Drag::B : Drag::Body;
            return true;
        }
    }
    return false;
}

void Editor::finishSurface(sf::Vector2f point)
{
    point = clamp(point);
    if (!anchor || length(point - *anchor) < MIN_SEGMENT_LENGTH) return;
    surfaces.push_back({*anchor, point, tool == Tool::Mirror ? Material::Mirror : Material::Wall});
    selected = {ObjectKind::Surface, surfaces.size() - 1};
    anchor.reset();
    tool = Tool::Select;
}

void Editor::pointerDown(sf::Vector2f point)
{
    pointer = point;
    if (!contains(point)) return;
    if (anchor) {
        finishSurface(point);
        finishingClick = true;
        return;
    }
    // Existing objects can be edited even while a placement tool is active.
    if (selectAt(point)) { tool = Tool::Select; return; }
    selected = {};
    if (tool == Tool::Laser) addLaser(point);
    else if (tool == Tool::Mirror || tool == Tool::Wall) {
        anchor = clamp(point);
        pressPoint = point;
        drawingPress = true;
    }
}

void Editor::pointerMove(sf::Vector2f point)
{
    pointer = point;
    const auto p = clamp(point);
    if (drag == Drag::Source) lasers[selected.index].setPosition(clamp(p - grabOffset));
    else if (drag == Drag::Aim) lasers[selected.index].setAimPoint(p);
    else if (drag == Drag::A || drag == Drag::B) {
        auto& s = surfaces[selected.index];
        const auto other = drag == Drag::A ? s.b : s.a;
        if (length(p - other) >= MIN_SEGMENT_LENGTH) {
            (drag == Drag::A ? s.a : s.b) = p;
        }
    } else if (drag == Drag::Body) {
        auto delta = p - pressPoint;
        const auto lo = bounds.position + sf::Vector2f(2.f, 2.f);
        const auto hi = bounds.position + bounds.size - sf::Vector2f(2.f, 2.f);
        delta.x = std::clamp(delta.x, lo.x - std::min(originalA.x, originalB.x),
                             hi.x - std::max(originalA.x, originalB.x));
        delta.y = std::clamp(delta.y, lo.y - std::min(originalA.y, originalB.y),
                             hi.y - std::max(originalA.y, originalB.y));
        surfaces[selected.index].a = originalA + delta;
        surfaces[selected.index].b = originalB + delta;
    }
}

void Editor::pointerUp(sf::Vector2f point)
{
    pointerMove(point);
    if (paletteDrag) {
        paletteDrag = false;
        if (contains(point)) {
            if (tool == Tool::Laser) addLaser(point);
            else if (tool == Tool::Mirror || tool == Tool::Wall) anchor = clamp(point);
        }
    } else if (drawingPress && !finishingClick && contains(point) &&
               length(point - pressPoint) >= MIN_SEGMENT_LENGTH) {
        finishSurface(point);
    }
    drag = Drag::None;
    drawingPress = finishingClick = false;
}

void Editor::deleteSelected()
{
    cancel();
    if (selected.kind == ObjectKind::Laser) lasers.erase(lasers.begin() + selected.index);
    else if (selected.kind == ObjectKind::Surface) surfaces.erase(surfaces.begin() + selected.index);
    selected = {};
}

std::optional<Surface> Editor::preview() const
{
    if (!anchor) return std::nullopt;
    return Surface{*anchor, clamp(pointer), tool == Tool::Mirror ? Material::Mirror : Material::Wall};
}

const char* Editor::hint() const
{
    if (anchor) return "Move to the other endpoint and click to finish.  Esc cancels.";
    if (paletteDrag) return "Drop onto the canvas to place.  Esc cancels.";
    if (tool == Tool::Laser) return "Click the canvas to place a laser, then drag its aim handle.";
    if (tool == Tool::Mirror || tool == Tool::Wall) return "Click two endpoints, or drag across the canvas to draw.";
    if (selected.kind == ObjectKind::Surface) return "Drag either endpoint to resize / rotate. Drag the middle to move.";
    if (selected.kind == ObjectKind::Laser) return "Drag the source to move. Drag the small handle to aim.";
    return "Choose a tool on the right, or click an object to edit it.";
}
