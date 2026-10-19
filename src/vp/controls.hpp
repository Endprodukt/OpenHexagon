// The controls: the game's functions by what is laid out on them
// (Settings::button, vpbind.h: read directly), and the menus' own, which no
// laying out touches.
//
// The original moves the player one way or the other at a fixed speed per
// tick (CPlayer::updateInputMovement), from two buttons. The spinner moves
// it 1:1 (the user's rule, 2026-10-18): what it turned in a step is shared
// out over that step's ticks and becomes their speed
// (HexagonGame::vpSetSpin), so the player stops when the spinner stops and
// is never behind it. The original's wall pushing still works, as for keys.
#pragma once

#include "settings.hpp"

typedef struct vp_input vp_input;

namespace vp {

struct Controls
{
    // Once per update step (60 Hz), before the ticks.
    void step(vp_input* in, const Settings& s);

    // Once per game tick: the keys' direction for HexagonGame::setInputs,
    // and the spinner's turn for this tick (degrees, + clockwise; 0 while a
    // key turns). `ticks`: the ticks of this step.
    void tick(int ticks, bool& ccw, bool& cw, float& spinDeg) const;

    void reset();

    bool left{false}, right{false};     // held
    bool focus{false}, swap{false};     // held
    bool focusPressed{false}, swapPressed{false}, backPressed{false};
    bool leftPressed{false}, rightPressed{false};
    bool start1{false}, start2{false};  // pressed

    // The menus' own (vpbind.h), whatever is laid out: pressed.
    bool ok{false};                     // START 1, Enter, the keypad's Enter
    bool alt{false};                    // START 2: a menu's second choice
    bool menuBack{false};               // Backspace (home mode, a PC's)
    bool up{false}, down{false};        // the arrows, any stick or hat
    bool menuLeft{false}, menuRight{false};
    int spin{0};                        // spinner counts this step, signed as set
    float spinDeg{0.f};                 // the same in degrees of the player's turn

    // A press is reported once. vp_run catches up a late frame with more
    // steps on the same input, and the SDK says "pressed" in each of them:
    // one press went through the title and started a level at once
    // (2026-10-18). A press is latched until its control is let go.
    enum { E_START1, E_START2, E_COUNT };
    bool latched[E_COUNT]{};
};

// One full turn of a 1200-count spinner at sensitivity 1 is a full turn of
// the player: the service menu's spinner page shows it.
inline constexpr float DEG_PER_COUNT = 360.f / 1200.f;

// The menus: one step (a level, a letter) per this much spinner turn, in
// the player's degrees as above.
inline constexpr float MENU_STEP_DEG = 50.f;

} // namespace vp
