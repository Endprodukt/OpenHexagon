// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#pragma once

// VectorPie: controls come from advmame.rc, the key and mouse converters
// are left out.

namespace ssvs::Input {
class Trigger;
class Combo;
} // namespace ssvs::Input

namespace sf {
template <typename T>
class Vector2;

using Vector2f = Vector2<float>;

class Color;
} // namespace sf


namespace Json {
class Value;
}

namespace ssvuj {
using Obj = Json::Value;

template <typename>
struct Converter;
} // namespace ssvuj

namespace ssvuj {


template <>
struct Converter<sf::Vector2f>
{
    using T = sf::Vector2f;

    static void fromObj(const Obj& mObj, T& mValue);
    static void toObj(Obj& mObj, const T& mValue);
};

template <>
struct Converter<sf::Color>
{
    using T = sf::Color;

    static void fromObj(const Obj& mObj, T& mValue);
    static void toObj(Obj& mObj, const T& mValue);
};

#if 0 // VectorPie
template <>
struct Converter<ssvs::Input::Trigger>
{
    using T = ssvs::Input::Trigger;

    static void fromObj(const Obj& mObj, T& mValue);
    static void toObj(Obj& mObj, const T& mValue);
};

template <>
struct Converter<sf::Keyboard::Key>
{
    using T = sf::Keyboard::Key;

    static void fromObj(const Obj& mObj, T& mValue);
    static void toObj(Obj& mObj, const T& mValue);
};

template <>
struct Converter<sf::Mouse::Button>
{
    using T = sf::Mouse::Button;

    static void fromObj(const Obj& mObj, T& mValue);
    static void toObj(Obj& mObj, const T& mValue);
};

template <>
struct Converter<ssvs::Input::Combo>
{
    using T = ssvs::Input::Combo;

    static void fromObj(const Obj& mObj, T& mValue);
    static void toObj(Obj& mObj, const T& mValue);
};
#endif

} // namespace ssvuj
