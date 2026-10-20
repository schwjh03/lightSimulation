#include "laser/beam.hpp"
#include <cmath>

Beam::Beam(sf::Vector2f origin, sf::Vector2f aimPoint) : origin(origin), aimPoint(aimPoint) {}

void Beam::draw(sf::RenderTarget& target, const std::vector<Surface>& surfaces,
                sf::FloatRect bounds) const
{
    for (const auto& section : traceBeam(origin, aimPoint - origin, surfaces, bounds)) {
        const auto offset = section.b - section.a;
        sf::RectangleShape glow({length(offset), 5.f});
        glow.setOrigin({0.f, 2.5f});
        glow.setPosition(section.a);
        glow.setRotation(sf::radians(std::atan2(offset.y, offset.x)));
        glow.setFillColor(sf::Color(color.r, color.g, color.b, 30));
        target.draw(glow);
        const sf::Vertex line[] = {{section.a, color}, {section.b, color}};
        target.draw(line, 2, sf::PrimitiveType::Lines);
    }
}

void Beam::setAimPoint(sf::Vector2f target)
{
    if (length(target - origin) > 0.001f) aimPoint = target;
}

void Beam::setOrigin(sf::Vector2f target)
{
    aimPoint += target - origin;
    origin = target;
}

void Beam::setColor(sf::Color newColor) { color = newColor; }
