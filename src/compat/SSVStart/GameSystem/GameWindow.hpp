// VectorPie port: what the game core asks of SSVStart's GameWindow. There is
// no window on the board: drawing goes through src/vp/render.cpp and the
// controls through vp_input (src/vp/controls.cpp), so these say "no timer,
// no touch, no keys". HexagonGame keeps a pointer to one so that its
// `window != nullptr` checks keep meaning "drawn and heard", and a null
// pointer still means headless (replay checks, the self test).
#pragma once

#include <SFML/System/Vector2.hpp>

#include <vector>

namespace ssvs {

using KKey = int;
using MBtn = int;

class GameWindow
{
public:
    struct TimerBase
    {
        void reset() {}
    };

    struct RenderWindow
    {
        [[nodiscard]] sf::Vector2u getSize() const
        {
            return {1024u, 768u};
        }
    };

    struct InputState
    {
        [[nodiscard]] bool operator[](int) const
        {
            return false;
        }
    };

    [[nodiscard]] bool hasTimer() const
    {
        return false;
    }

    [[nodiscard]] TimerBase& getTimerBase()
    {
        return _timer;
    }

    [[nodiscard]] const std::vector<sf::Vector2f>& getFingerDownPositions() const
    {
        return _fingers;
    }

    [[nodiscard]] const RenderWindow& getRenderWindow() const
    {
        return _renderWindow;
    }

    [[nodiscard]] InputState getInputState() const
    {
        return {};
    }

private:
    TimerBase _timer;
    RenderWindow _renderWindow;
    std::vector<sf::Vector2f> _fingers;
};

} // namespace ssvs
