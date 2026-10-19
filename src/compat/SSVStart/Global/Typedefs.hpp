// VectorPie port: the vector typedefs of SSVStart's Typedefs.hpp (the
// keyboard, mouse and touch parts belong to SFML and are left out).
#pragma once

#include <SFML/System/Vector2.hpp>

namespace ssvs {

template <typename T>
using Vec2 = sf::Vector2<T>;

using Vec2i = Vec2<int>;
using Vec2f = Vec2<float>;
using Vec2u = Vec2<unsigned int>;

} // namespace ssvs
