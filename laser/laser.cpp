#include "laser/laser.hpp"
#include <cmath>

Laser::Laser(sf::Vector2f position, sf::Vector2f aimPoint, sf::Color color)
    : beam(position, aimPoint) { beam.setColor(color); }

sf::Vector2f Laser::aimHandle() const
{
    const auto direction = beam.getAimPoint() - beam.getOrigin();
    return beam.getOrigin() + direction / length(direction) * 46.f;
}

void Laser::drawBeam(sf::RenderTarget& target, const std::vector<Surface>& surfaces,
                     sf::FloatRect bounds) const { beam.draw(target, surfaces, bounds); }

void Laser::drawNode(sf::RenderTarget& target, bool selected) const
{
    const auto p = getPosition();
    const auto handle = aimHandle();
    sf::CircleShape source(12.f, 32);
    source.setOrigin({12.f, 12.f});
    source.setPosition(p);
    source.setFillColor(sf::Color(24, 33, 45));
    source.setOutlineThickness(2.f);
    source.setOutlineColor(selected ? sf::Color::White : getColor());
    target.draw(source);
    sf::CircleShape core(5.f, 24);
    core.setOrigin({5.f, 5.f});
    core.setPosition(p);
    core.setFillColor(getColor());
    target.draw(core);
    if (selected) {
        const sf::Vertex guide[] = {{p, sf::Color(120, 139, 163)}, {handle, sf::Color(120, 139, 163)}};
        target.draw(guide, 2, sf::PrimitiveType::Lines);
        sf::CircleShape knob(6.f, 24);
        knob.setOrigin({6.f, 6.f});
        knob.setPosition(handle);
        knob.setFillColor(getColor());
        knob.setOutlineThickness(2.f);
        knob.setOutlineColor(sf::Color::White);
        target.draw(knob);
    }
}
