// VectorPie port: sf::Vertex, the corner of a filled triangle. The vector
// renderer (src/vp/render.cpp) turns the triangles back into outlines.
#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

namespace sf {

struct Vertex
{
    Vector2f position{};
    Color color{255, 255, 255};
    Vector2f texCoords{};
};

} // namespace sf
