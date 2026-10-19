// VectorPie port: sf::View as SSVStart's Camera uses it: a center, a size
// and a rotation in degrees. src/vp/render.cpp maps it onto the board.
#pragma once

#include <SFML/System/Vector2.hpp>

namespace sf {

class View
{
private:
    Vector2f _center{0.f, 0.f};
    Vector2f _size{1000.f, 1000.f};
    float _rotation{0.f}; // degrees, 0 .. 360

public:
    View() = default;
    View(const Vector2f& center, const Vector2f& size)
        : _center{center}, _size{size}
    {}

    void setCenter(const Vector2f& c) { _center = c; }
    void setSize(const Vector2f& s) { _size = s; }

    void setRotation(float deg)
    {
        _rotation = deg - 360.f * static_cast<float>(static_cast<int>(deg / 360.f));
        if(_rotation < 0.f)
        {
            _rotation += 360.f;
        }
    }

    void rotate(float deg) { setRotation(_rotation + deg); }
    void zoom(float factor) { _size = _size * factor; }

    [[nodiscard]] const Vector2f& getCenter() const { return _center; }
    [[nodiscard]] const Vector2f& getSize() const { return _size; }
    [[nodiscard]] float getRotationDeg() const { return _rotation; }
};

} // namespace sf
