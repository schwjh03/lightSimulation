#pragma once

#include "laser/beam.hpp"

class Laser {
public:
    Laser(sf::Vector2f position, sf::Vector2f aimPoint,
          sf::Color color = sf::Color(72, 237, 147));
    void drawBeam(sf::RenderTarget& target, const std::vector<Surface>& surfaces,
                  sf::FloatRect bounds) const;
    void drawNode(sf::RenderTarget& target, bool selected) const;
    void setAimPoint(sf::Vector2f target) { beam.setAimPoint(target); }
    void setPosition(sf::Vector2f target) { beam.setOrigin(target); }
    void setColor(sf::Color color) { beam.setColor(color); }
    sf::Vector2f getPosition() const { return beam.getOrigin(); }
    sf::Color getColor() const { return beam.getColor(); }
    sf::Vector2f aimHandle() const;
private:
    Beam beam;
};
