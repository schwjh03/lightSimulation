#pragma once

#include "optics.hpp"

class Beam {
public:
    Beam(sf::Vector2f origin, sf::Vector2f aimPoint);
    void draw(sf::RenderTarget& target, const std::vector<Surface>& surfaces,
              sf::FloatRect bounds) const;
    void setAimPoint(sf::Vector2f target);
    void setOrigin(sf::Vector2f target);
    void setColor(sf::Color newColor);
    sf::Vector2f getOrigin() const { return origin; }
    sf::Vector2f getAimPoint() const { return aimPoint; }
    sf::Color getColor() const { return color; }
private:
    sf::Vector2f origin;
    sf::Vector2f aimPoint;
    sf::Color color = sf::Color(72, 237, 147);
};
