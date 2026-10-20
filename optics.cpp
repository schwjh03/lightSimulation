#include "optics.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float epsilon = 0.001f;
float dot(sf::Vector2f a, sf::Vector2f b) { return a.x * b.x + a.y * b.y; }
float cross(sf::Vector2f a, sf::Vector2f b) { return a.x * b.y - a.y * b.x; }

bool intersect(sf::Vector2f origin, sf::Vector2f direction,
               sf::Vector2f a, sf::Vector2f b, float& t)
{
    const auto edge = b - a;
    const float divisor = cross(direction, edge);
    if (std::abs(divisor) < 0.000001f) return false;
    t = cross(a - origin, edge) / divisor;
    const float u = cross(a - origin, direction) / divisor;
    return t > epsilon && u >= -0.00001f && u <= 1.00001f;
}
}

float length(sf::Vector2f v) { return std::hypot(v.x, v.y); }

float distanceToSegment(sf::Vector2f point, sf::Vector2f a, sf::Vector2f b)
{
    const auto edge = b - a;
    const float squared = dot(edge, edge);
    if (squared < epsilon) return length(point - a);
    const float t = std::clamp(dot(point - a, edge) / squared, 0.f, 1.f);
    return length(point - (a + edge * t));
}

std::vector<RaySection> traceBeam(sf::Vector2f origin, sf::Vector2f direction,
                                const std::vector<Surface>& surfaces,
                                sf::FloatRect bounds, unsigned maxReflections)
{
    std::vector<RaySection> result;
    const float magnitude = length(direction);
    if (magnitude < epsilon || !bounds.contains(origin)) return result;
    direction /= magnitude;
    const auto lo = bounds.position;
    const auto hi = lo + bounds.size;
    const Surface border[] = {
        {lo, {hi.x, lo.y}, Material::Wall},
        {{hi.x, lo.y}, hi, Material::Wall},
        {hi, {lo.x, hi.y}, Material::Wall},
        {{lo.x, hi.y}, lo, Material::Wall}
    };
    for (unsigned bounce = 0; bounce <= maxReflections; ++bounce) {
        float nearest = std::numeric_limits<float>::infinity();
        const Surface* hit = nullptr;
        for (const auto& edge : border) {
            float t;
            if (intersect(origin, direction, edge.a, edge.b, t) && t < nearest) {
                nearest = t;
                hit = &edge;
            }
        }
        for (const auto& surface : surfaces) {
            float t;
            // At a shared endpoint an absorbing wall wins a tie.
            if (intersect(origin, direction, surface.a, surface.b, t) &&
                (t < nearest - epsilon ||
                 (std::abs(t - nearest) <= epsilon && surface.material == Material::Wall))) {
                nearest = t;
                hit = &surface;
            }
        }
        if (!hit) break;
        const auto endpoint = origin + direction * nearest;
        result.push_back({origin, endpoint});
        if (hit->material == Material::Wall || bounce == maxReflections) break;
        const auto edge = hit->b - hit->a;
        const float edgeLength = length(edge);
        if (edgeLength < epsilon) break;
        const sf::Vector2f normal(-edge.y / edgeLength, edge.x / edgeLength);
        direction -= normal * (2.f * dot(direction, normal));
        origin = endpoint;
    }
    return result;
}
