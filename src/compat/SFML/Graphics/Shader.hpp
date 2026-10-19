// VectorPie port: an sf::Shader that does nothing. The original's shader
// Lua functions (shdr_*) stay; HGAssets has no shaders, so every id is
// invalid and the calls return early as they do in the original's headless
// mode.
#pragma once

#include <SFML/Graphics/Glsl.hpp>

#include <string>

namespace sf {

class Shader
{
public:
    enum class Type
    {
        Vertex,
        Geometry,
        Fragment
    };

    template <typename... Ts>
    void setUniformUnsafe(const std::string&, const Ts&...)
    {}

    template <typename... Ts>
    void setUniform(const std::string&, const Ts&...)
    {}
};

} // namespace sf
