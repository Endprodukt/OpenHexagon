// VectorPie port: the GLSL vector types the original's shader Lua API names.
// There are no shaders on a vector board; see Shader.hpp.
#pragma once

#include <SFML/System/Vector2.hpp>

namespace sf::Glsl {

struct Vec3
{
    float x{}, y{}, z{};
};

struct Vec4
{
    float x{}, y{}, z{}, w{};
};

struct Ivec2
{
    int x{}, y{};
};

struct Ivec3
{
    int x{}, y{}, z{};
};

struct Ivec4
{
    int x{}, y{}, z{}, w{};
};

using Vec2 = sf::Vector2f;

} // namespace sf::Glsl
