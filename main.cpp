#include <SFML/Graphics.hpp>
#include "values.hpp"
#include "laser/laser.hpp"

int main()
{

    //Initialize the window
    sf::RenderWindow window(
        sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}),
        "Simple Laser",
        sf::Style::Titlebar | sf::Style::Close
    );

    window.setFramerateLimit(120);

    // Initialize the laser
    const sf::Vector2f laserPosition(150.f, 350.f);
    const sf::Vector2f initialAimPoint(laserPosition.x + 1.f, laserPosition.y);

    Laser laser(laserPosition, initialAimPoint);

    // Initialize the buttons

    sf::Vertex separator[] =
    {
        sf::Vertex{{0, BUFFER_SIZE}, sf::Color::White},
        sf::Vertex{{WINDOW_WIDTH, BUFFER_SIZE}, sf::Color::White}
    };

    bool followMouse = true;
    
    sf::RectangleShape followButton(sf::Vector2f(100.f, 50.f));

    followButton.setPosition({WINDOW_WIDTH / 2.f - 50.f, 20.f});
    followButton.setFillColor(sf::Color::Green);

    followButton.setOutlineThickness(2.f);
    followButton.setOutlineColor(sf::Color::White);

    while (window.isOpen())
    {
        // Handle events
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (const auto* mouseButton = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouseButton->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f clickPosition = window.mapPixelToCoords(mouseButton->position);

                    if (followButton.getGlobalBounds().contains(clickPosition)) 
                    {
                        followMouse = !followMouse;
                        if (followMouse)
                        {
                            followButton.setFillColor(sf::Color::Green);
                        }
                        else
                        {
                            followButton.setFillColor(sf::Color::Red);
                        }
                    }
                }
            }
        }

        if (!window.isOpen())
        {
            break;
        }

        if (followMouse)
        {
            const sf::Vector2f mousePosition = 
                window.mapPixelToCoords(sf::Mouse::getPosition(window));

            laser.setAimPoint(mousePosition);
        }

        // Clear screen
        window.clear(sf::Color(20, 20, 24));

        // Draw the separator below the button area every frame
        window.draw(separator, 2, sf::PrimitiveType::Lines);

        // Draw laser source
        laser.draw(window);

        // Draw follow button
        window.draw(followButton);

        // Display everything
        window.display();
    }

    return 0;
}
