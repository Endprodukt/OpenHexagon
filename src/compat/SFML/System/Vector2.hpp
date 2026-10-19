// VectorPie port: the small part of SFML's sf::Vector2 the Open Hexagon core
// uses. SFML itself is not linked; drawing goes through the VectorPie SDK.
#pragma once

#include <cmath>

namespace sf {

template <typename T>
struct Vector2
{
    T x{};
    T y{};

    constexpr Vector2() = default;
    constexpr Vector2(T mX, T mY) : x(mX), y(mY) {}

    template <typename U>
    constexpr explicit Vector2(const Vector2<U>& v)
        : x(static_cast<T>(v.x)), y(static_cast<T>(v.y))
    {}

    [[nodiscard]] constexpr T cross(const Vector2& v) const
    {
        return x * v.y - y * v.x;
    }

    [[nodiscard]] constexpr T dot(const Vector2& v) const
    {
        return x * v.x + y * v.y;
    }

    [[nodiscard]] T length() const { return std::sqrt(x * x + y * y); }

    [[nodiscard]] constexpr T lengthSq() const { return x * x + y * y; }

    [[nodiscard]] Vector2 normalized() const
    {
        const T l = length();
        return Vector2{x / l, y / l};
    }
};

template <typename T>
constexpr Vector2<T> operator-(const Vector2<T>& a) { return {-a.x, -a.y}; }
template <typename T>
constexpr Vector2<T> operator+(const Vector2<T>& a, const Vector2<T>& b) { return {a.x + b.x, a.y + b.y}; }
template <typename T>
constexpr Vector2<T> operator-(const Vector2<T>& a, const Vector2<T>& b) { return {a.x - b.x, a.y - b.y}; }
template <typename T>
constexpr Vector2<T> operator*(const Vector2<T>& a, T s) { return {a.x * s, a.y * s}; }
template <typename T>
constexpr Vector2<T> operator*(T s, const Vector2<T>& a) { return {a.x * s, a.y * s}; }
template <typename T>
constexpr Vector2<T> operator/(const Vector2<T>& a, T s) { return {a.x / s, a.y / s}; }
template <typename T>
constexpr Vector2<T>& operator+=(Vector2<T>& a, const Vector2<T>& b) { a.x += b.x; a.y += b.y; return a; }
template <typename T>
constexpr Vector2<T>& operator-=(Vector2<T>& a, const Vector2<T>& b) { a.x -= b.x; a.y -= b.y; return a; }
template <typename T>
constexpr Vector2<T>& operator*=(Vector2<T>& a, T s) { a.x *= s; a.y *= s; return a; }
template <typename T>
constexpr Vector2<T>& operator/=(Vector2<T>& a, T s) { a.x /= s; a.y /= s; return a; }
template <typename T>
constexpr bool operator==(const Vector2<T>& a, const Vector2<T>& b) { return a.x == b.x && a.y == b.y; }
template <typename T>
constexpr bool operator!=(const Vector2<T>& a, const Vector2<T>& b) { return !(a == b); }

using Vector2i = Vector2<int>;
using Vector2u = Vector2<unsigned int>;
using Vector2f = Vector2<float>;

} // namespace sf
