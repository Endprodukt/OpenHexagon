// The controls: see controls.hpp.
#include "controls.hpp"

#include "vpbind.h"

#include <SDL.h>
#include <vectorpie/vp.h>

#include <algorithm>
#include <cmath>

namespace vp {

void Controls::step(vp_input* in, const Settings& s)
{
    // The SDK's (the starts): latched, as it says "pressed" again in the
    // steps that catch up a late frame. vpbind's are read once a step.
    const auto pressed = [&](const char* a, int e)
    {
        const bool sdk = vp_input_pressed(in, a) != 0;
        const bool report = sdk && !latched[e];
        latched[e] = report || (latched[e] && vp_input_held(in, a) != 0);
        return report;
    };
    const auto held = [&](const std::string& a) { return vpb_held(a.c_str()) != 0; };
    const auto down_now = [&](const std::string& a) { return vpb_pressed(a.c_str()) != 0; };

    left = held(s.button[BUTTON_LEFT]);
    right = held(s.button[BUTTON_RIGHT]);
    leftPressed = down_now(s.button[BUTTON_LEFT]);
    rightPressed = down_now(s.button[BUTTON_RIGHT]);
    focus = held(s.button[BUTTON_FOCUS]);
    swap = held(s.button[BUTTON_SWAP]);
    focusPressed = down_now(s.button[BUTTON_FOCUS]);
    swapPressed = down_now(s.button[BUTTON_SWAP]);
    backPressed = down_now(s.button[BUTTON_BACK]);
    start1 = pressed("start1", E_START1);
    start2 = pressed("start2", E_START2);

    ok = start1 || vpb_key_pressed(SDL_SCANCODE_RETURN) || vpb_key_pressed(SDL_SCANCODE_KP_ENTER);
    alt = start2;
    menuBack = vpb_key_pressed(SDL_SCANCODE_BACKSPACE);
    up = vpb_menu_up();
    down = vpb_menu_down();
    menuLeft = vpb_menu_left_pressed();
    menuRight = vpb_menu_right_pressed();

    spin = vp_input_delta(in, "p1_dialx") * (s.reversed ? -1 : 1);
    spinDeg = static_cast<float>(spin) * DEG_PER_COUNT * s.sensitivity;
}

void Controls::tick(int ticks, bool& ccw, bool& cw, float& deg) const
{
    ccw = left;
    cw = right;
    // Keys win: whoever holds one takes over from the spinner.
    deg = (left || right || ticks <= 0) ? 0.f : spinDeg / static_cast<float>(ticks);
}

void Controls::reset()
{
    spin = 0;
    spinDeg = 0.f;
}

} // namespace vp
