#include <SFML/Graphics.hpp>
#include <cmath>

int main()
{
    sf::RenderWindow window(
        sf::VideoMode(1000, 700),
        "Simple Laser"
    );

    window.setFramerateLimit(60);

    const sf::Vector2f laserPosition(150.f, 350.f);
    sf::Vector2f direction(1.f, 0.f);

    bool followMouse = true;

    sf::CircleShape source(6.f);
    source.setOrigin(6.f, 6.f);
    source.setPosition(laserPosition);
    source.setFillColor(sf::Color::Red);
    sf::RectangleShape followButton(sf::Vector2f(100.f, 50.f));

    followButton.setPosition(1000.f / 2.f - 50.f, 20.f);
    followButton.setFillColor(sf::Color::Green);

    followButton.setOutlineThickness(2.f);
    followButton.setOutlineColor(sf::Color::White);

    while (window.isOpen())
    {
        // Handle events
        sf::Event event;

        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
            }

            if(event.type == sf::Event::MouseButtonPressed) 
            {
                if(event.mouseButton.button == sf::Mouse::Left) 
                {
                    sf::Vector2f clickPosition = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));

                    if(followButton.getGlobalBounds().contains(clickPosition)) 
                    {
                        followMouse = !followMouse;
                        if(followMouse == true)
                        {
                            followButton.setFillColor(sf::Color::Green);
                        }
                        else if(followMouse == false)
                        {
                            followButton.setFillColor(sf::Color::Red);
                        }
                    }
                }
            }
        }

        if(followMouse)
        {
            sf::Vector2f mouse = 
            window.mapPixelToCoords(sf::Mouse::getPosition(window));

            sf::Vector2f offset = mouse - laserPosition;

            float length = std::sqrt(offset.x * offset.x + offset.y * offset.y);

            if(length > 0.f)
            {
                direction.x = offset.x / length;
                direction.y = offset.y / length;
            }
        }

        // Create the laser beam
        sf::Vertex beam[] =
        {
            sf::Vertex(
                laserPosition,
                sf::Color::Red
            ),

            sf::Vertex(
                laserPosition + direction * 2000.f,
                sf::Color::Red
            )
        };

        // Clear screen
        window.clear(sf::Color(20, 20, 24));

        // Draw laser
        window.draw(
            beam,
            2,
            sf::Lines
        );

        // Draw laser source
        window.draw(source);

        // Draw follow button
        window.draw(followButton);

        // Display everything
        window.display();
    }

    return 0;
}