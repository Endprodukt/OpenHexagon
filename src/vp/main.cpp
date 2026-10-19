// Open Hexagon for VectorPie: the vector monitor port.
//
// Controls, by their advmame.rc names (changed in the service menu, F2):
//   p1_dialx              the spinner
//   p1_left / p1_right    turn on buttons or a stick
//   p1_button1            focus (slow, as the original's Shift)
//   p1_button2            swap
//   p1_button3            back to the level choice (home mode)
//   coin1 / coin2         a credit (arcade mode)
//   start1                start, restart
//
// The game logic runs at the original's 240 ticks a second (Config::
// TIME_STEP), four ticks in every 60 Hz update; the board is drawn at the
// service menu's frame rate.
#include "app.hpp"

#include <vectorpie/vp.h>

#include <cstdlib>
#include <cstring>

namespace {

void update(vp_game* g, float dt) { vp::appUpdate(g, dt); }
void draw(vp_game* g, vp_dvg* d) { vp::appDraw(g, d); }
int init(vp_game* g) { return vp::appInit(g); }
void shutdown(vp_game* g) { vp::appShutdown(g); }

} // namespace

int main(int argc, char** argv)
{
    // The VectorPie menu starts a game as <name>_dvg or <name>_sdl.
    if(!std::getenv("VP_OUTPUT") && argc > 0)
    {
        const std::size_t n = std::strlen(argv[0]);
        const char* output = n >= 4 && !std::strcmp(argv[0] + n - 4, "_sdl")   ? "sdl"
                             : n >= 4 && !std::strcmp(argv[0] + n - 4, "_dvg") ? "dvg"
                                                                               : nullptr;
#ifdef _WIN32
        if(output) _putenv_s("VP_OUTPUT", output);
#else
        if(output) setenv("VP_OUTPUT", output, 1);
#endif
    }

    const vp::Settings& s = vp::appBootSettings();

    vp_game_desc desc{};
    desc.title = "Open Hexagon";
    desc.init = init;
    desc.update = update;
    desc.draw = draw;
    desc.shutdown = shutdown;
    desc.update_hz = 60;
    desc.frame_hz = 60;
    desc.frame_hz_board = s.frameHz;
    return vp_run(&desc, argc, argv);
}
