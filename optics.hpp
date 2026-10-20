#pragma once

#include <SFML/Graphics.hpp>
#include <vector>

enum class Material { Mirror, Wall };
struct Surface {
    sf::Vector2f a;
    sf::Vector2f b;
    Material material = Material::Mirror;
};
struct RaySection { sf::Vector2f a, b; };

float length(sf::Vector2f v);
float distanceToSegment(sf::Vector2f point, sf::Vector2f a, sf::Vector2f b);
std::vector<RaySection> traceBeam(sf::Vector2f origin, sf::Vector2f direction,
                                const std::vector<Surface>& surfaces,
                                sf::FloatRect bounds, unsigned maxReflections = 64);
