#pragma once

#include "laser/laser.hpp"
#include <optional>

enum class Tool { Select, Laser, Mirror, Wall };
enum class ObjectKind { None, Laser, Surface };
struct Selection { ObjectKind kind = ObjectKind::None; std::size_t index = 0; };

class Editor {
public:
    explicit Editor(sf::FloatRect bounds) : bounds(bounds) {}
    void setTool(Tool next);
    void setColor(sf::Color next);
    void beginPaletteDrag(Tool next);
    void pointerDown(sf::Vector2f point);
    void pointerMove(sf::Vector2f point);
    void pointerUp(sf::Vector2f point);
    void cancel();
    void deleteSelected();
    bool contains(sf::Vector2f point) const { return bounds.contains(point); }
    sf::Vector2f clamp(sf::Vector2f point) const;
    std::optional<Surface> preview() const;
    const std::vector<Laser>& getLasers() const { return lasers; }
    const std::vector<Surface>& getSurfaces() const { return surfaces; }
    Selection getSelection() const { return selected; }
    Tool getTool() const { return tool; }
    sf::Color getColor() const { return color; }
    bool isPaletteDragging() const { return paletteDrag; }
    sf::Vector2f getPointer() const { return pointer; }
    const char* hint() const;
private:
    enum class Drag { None, Source, Aim, Body, A, B };
    void addLaser(sf::Vector2f point);
    void finishSurface(sf::Vector2f point);
    bool selectAt(sf::Vector2f point);
    sf::FloatRect bounds;
    std::vector<Laser> lasers;
    std::vector<Surface> surfaces;
    Selection selected;
    Tool tool = Tool::Select;
    sf::Color color = sf::Color(72, 237, 147);
    Drag drag = Drag::None;
    bool paletteDrag = false;
    bool drawingPress = false;
    bool finishingClick = false;
    std::optional<sf::Vector2f> anchor;
    sf::Vector2f pointer, pressPoint, grabOffset, originalA, originalB;
};
