#ifndef LASER_LASER_HPP
#define LASER_LASER_HPP

#include "laser/beam.hpp"
#include <SFML/Graphics.hpp>

class Laser
{
public:
    Laser(sf::Vector2f position, sf::Vector2f aimPoint);

    void draw(sf::RenderWindow& window);
    void setAimPoint(sf::Vector2f target);

private:
    sf::Vector2f position;
    Beam beam;
};

#endif // LASER_LASER_HPP
