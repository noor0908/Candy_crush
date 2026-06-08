#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace CandyCrush {

struct Particle {
    sf::Vector2f position;
    sf::Vector2f velocity;
    sf::Color color;
    float size;
    float lifetime;     // Remaining life in seconds
    float maxLifetime;  // Starting life in seconds

    void update(float dt) {
        position += velocity * dt;
        velocity.y += 380.0f * dt; // Gravity effect
        lifetime -= dt;
    }

    void draw(sf::RenderWindow& window) const {
        if (lifetime <= 0) return;
        sf::RectangleShape shape(sf::Vector2f(size, size));
        shape.setPosition(position);
        sf::Color drawColor = color;
        // Fade out transition
        drawColor.a = static_cast<sf::Uint8>(255 * (lifetime / maxLifetime));
        shape.setFillColor(drawColor);
        window.draw(shape);
    }
};

struct FloatingText {
    std::string text;
    sf::Vector2f position;
    sf::Color color;
    float lifetime;
    float maxLifetime;
    unsigned int characterSize;

    void update(float dt) {
        position.y -= 50.0f * dt; // Float upwards
        lifetime -= dt;
    }

    void draw(sf::RenderWindow& window, const sf::Font& font) const {
        if (lifetime <= 0) return;
        sf::Text drawText;
        drawText.setFont(font);
        drawText.setString(text);
        drawText.setCharacterSize(characterSize);
        
        sf::Color drawColor = color;
        // Fade out transition
        drawColor.a = static_cast<sf::Uint8>(255 * (lifetime / maxLifetime));
        drawText.setFillColor(drawColor);
        drawText.setPosition(position);
        
        // Add subtle shadow/outline for readability
        sf::Text shadowText = drawText;
        sf::Color shadowColor = sf::Color::Black;
        shadowColor.a = drawColor.a;
        shadowText.setFillColor(shadowColor);
        shadowText.setPosition(position.x + 2, position.y + 2);
        
        window.draw(shadowText);
        window.draw(drawText);
    }
};

} // namespace CandyCrush
