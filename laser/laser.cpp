#include "laser/laser.hpp"

Laser::Laser(sf::Vector2f position, sf::Vector2f aimPoint)
    : position(position), beam(position, aimPoint)
{
}

void Laser::draw(sf::RenderWindow& window)
{
    beam.draw(window);
}

void Laser::setAimPoint(sf::Vector2f target)
{
    beam.setAimPoint(target);
}