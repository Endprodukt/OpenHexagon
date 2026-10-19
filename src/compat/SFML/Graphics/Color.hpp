// VectorPie port: sf::Color as the Open Hexagon core uses it.
#pragma once

#include <cstdint>

namespace sf {

struct Color
{
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};

    constexpr Color() = default;
    constexpr Color(std::uint8_t mR, std::uint8_t mG, std::uint8_t mB,
        std::uint8_t mA = 255)
        : r(mR), g(mG), b(mB), a(mA)
    {}

    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Magenta;
    static const Color Cyan;
    static const Color Transparent;
};

inline constexpr Color Color::Black{0, 0, 0};
inline constexpr Color Color::White{255, 255, 255};
inline constexpr Color Color::Red{255, 0, 0};
inline constexpr Color Color::Green{0, 255, 0};
inline constexpr Color Color::Blue{0, 0, 255};
inline constexpr Color Color::Yellow{255, 255, 0};
inline constexpr Color Color::Magenta{255, 0, 255};
inline constexpr Color Color::Cyan{0, 255, 255};
inline constexpr Color Color::Transparent{0, 0, 0, 0};

constexpr bool operator==(const Color& x, const Color& y)
{
    return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a;
}

constexpr bool operator!=(const Color& x, const Color& y)
{
    return !(x == y);
}

} // namespace sf
