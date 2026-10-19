// VectorPie port: the parts of sf::Sprite, sf::Texture, sf::Font and
// sf::Angle the original's particle code uses. A sprite is a position,
// rotation, scale and color; src/vp/render.cpp draws the original's star and
// circle particles as small vector shapes by the texture they name.
#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <string>

namespace sf {

struct Angle
{
    float deg{0.f};

    [[nodiscard]] constexpr float asDegrees() const { return deg; }
};

[[nodiscard]] constexpr Angle degrees(float d) { return Angle{d}; }
[[nodiscard]] constexpr Angle operator+(Angle a, Angle b) { return Angle{a.deg + b.deg}; }

class Texture
{
public:
    std::string name;   // VectorPie: "starParticle.png", "smallCircle.png"
    Vector2u size{32u, 32u};

    void setSmooth(bool) {}
    [[nodiscard]] Vector2u getSize() const { return size; }
};

class Font
{};

class Sprite
{
private:
    const Texture* _texture{nullptr};
    Vector2f _position{};
    Vector2f _origin{};
    Vector2f _scale{1.f, 1.f};
    Angle _rotation{};
    Color _color{255, 255, 255};

public:
    void setTexture(const Texture& t) { _texture = &t; }
    [[nodiscard]] const Texture* getTexture() const { return _texture; }

    void setPosition(const Vector2f& p) { _position = p; }
    [[nodiscard]] const Vector2f& getPosition() const { return _position; }

    void setOrigin(const Vector2f& o) { _origin = o; }
    [[nodiscard]] const Vector2f& getOrigin() const { return _origin; }

    void setScale(const Vector2f& s) { _scale = s; }
    [[nodiscard]] const Vector2f& getScale() const { return _scale; }

    void setRotation(Angle a) { _rotation = a; }
    [[nodiscard]] Angle getRotation() const { return _rotation; }

    void setColor(const Color& c) { _color = c; }
    [[nodiscard]] const Color& getColor() const { return _color; }
};

} // namespace sf
