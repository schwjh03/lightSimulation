#ifndef LASER_BEAM_HPP
#define LASER_BEAM_HPP

#include <SFML/Graphics.hpp>

class Beam
{
public:
    Beam(sf::Vector2f origin, sf::Vector2f aimPoint);

    void draw(sf::RenderWindow& window);
    void setAimPoint(sf::Vector2f target);

    void setColor(sf::Color newColor);

private:
    sf::Vector2f origin;
    sf::Vector2f aimPoint;
    sf::Color color = sf::Color::Red;
};

#endif // LASER_BEAM_HPP
