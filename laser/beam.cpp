#include "laser/beam.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

Beam::Beam(sf::Vector2f origin, sf::Vector2f aimPoint)
    : origin(origin), aimPoint(aimPoint)
{
}

void Beam::draw(sf::RenderWindow& window)
{
    const sf::Vector2f offset = aimPoint - origin;
    const float length = std::hypot(offset.x, offset.y);
    if (length == 0.f) {
        return;
    }

    // Reach beyond even the farthest window corner from the source.
    const auto size = window.getSize();
    const float farX = std::max(std::abs(origin.x),
                               std::abs(static_cast<float>(size.x) - origin.x));
    const float farY = std::max(std::abs(origin.y),
                               std::abs(static_cast<float>(size.y) - origin.y));
    const float beamLength = std::hypot(farX, farY) + 1.f;
    const sf::Vector2f endpoint = origin + (offset / length) * beamLength;
    const sf::Vertex vertices[] =
    {
        sf::Vertex{origin, sf::Color::Red},
        sf::Vertex{endpoint, sf::Color::Red}
    };

    window.draw(vertices, 2, sf::PrimitiveType::Lines);
}

void Beam::setAimPoint(sf::Vector2f target)
{
    // Keep the previous aim if the mouse is exactly on the source.
    if (target != origin) {
        aimPoint = target;
    }
}
