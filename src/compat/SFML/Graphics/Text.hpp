// VectorPie port: sf::Text as a string with a character size and color.
// The original builds its HUD strings in sf::Text objects (updateText);
// src/vp/render.cpp draws them with the SDK's stroke font.
#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>

namespace sf {

class String : public std::string
{
public:
    using std::string::string;
    String(const std::string& s) : std::string(s) {}

    [[nodiscard]] bool isEmpty() const { return empty(); }
};

class Text
{
private:
    String _string;
    unsigned int _characterSize{30};
    Color _fillColor{255, 255, 255};
    Vector2f _position{};
    Vector2f _origin{};

public:
    Text() = default;
    Text(const std::string& s, const Font&, unsigned int size = 30)
        : _string{s}, _characterSize{size}
    {}

    void setString(const std::string& s) { _string = s; }
    [[nodiscard]] const String& getString() const { return _string; }

    void setCharacterSize(unsigned int s) { _characterSize = s; }
    [[nodiscard]] unsigned int getCharacterSize() const { return _characterSize; }

    void setFillColor(const Color& c) { _fillColor = c; }
    [[nodiscard]] const Color& getFillColor() const { return _fillColor; }

    void setOutlineColor(const Color&) {}
    void setOutlineThickness(float) {}

    void setPosition(const Vector2f& p) { _position = p; }
    [[nodiscard]] const Vector2f& getPosition() const { return _position; }

    void setOrigin(const Vector2f& o) { _origin = o; }
};

} // namespace sf
