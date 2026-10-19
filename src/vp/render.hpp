// The picture on the vector board.
//
// Renderer::drawGame turns what HexagonGame::draw() filled (the original's
// background triangles, wall quads, pivot quads, cap and player triangles)
// into outlines, as src/vp/occlude.hpp describes, through the original's
// background camera (rotation, pulse zoom, 3D skew, shake). The 3D effect
// is a band under every shape, from the shape to its deepest layer, that
// hides what lies behind it; the original's text is drawn in the SDK's
// stroke font.
//
// The ui:: helpers draw the menus on a 384 x 288 screen (4:3, y down), as
// the Tempest 2000 port's service menu does on its own.
#pragma once

#include "occlude.hpp"
#include "settings.hpp"

#include <string>
#include <vector>

typedef struct vp_dvg vp_dvg;

namespace hg {
class HexagonGame;
class StyleData;
}

namespace vp {

class Renderer
{
public:
    // `hud`: the original's text (time, status, messages); off for the
    // level choice's preview.
    void drawGame(hg::HexagonGame& game, vp_dvg* d, const Settings& s,
        bool hud = true);

    // The menus' background: a level's style as the original's menu shows
    // it (drawBackgroundMenu), turning, with the walls left out.
    void drawMenuBackground(const hg::StyleData& style, unsigned int sides,
        float rotationDeg, vp_dvg* d, const Settings& s);

    // The title screen: the logo as solid block letters over a tunnel of
    // walls, pulsing `beatHz` times a second.
    void drawTitle(double time, float beatHz, vp_dvg* d, const Settings& s);

    [[nodiscard]] const Occluder::Stats& lastStats() const { return _stats; }

    // For the self test: how the walls drawn now come out on the beam (the
    // darkest one's brightness 0..255, -1 without walls) and whether the
    // style is drawn inverted. Call after HexagonGame::draw().
    struct Probe
    {
        int darkestWall;
        bool inverted;
        int walls;
    };

    static Probe probe(hg::HexagonGame& game, const Settings& s);

    // For the self test, after drawGame: walls fully on the board of which
    // less than a third of the outline was drawn.
    [[nodiscard]] int mostlyHiddenWalls() const;

private:
    Occluder _occ;
    Occluder::Stats _stats{};

    // Simpler drawing for a level the board cannot draw in time (Arcadia's
    // full height bars, 2026-10-18: it flickered). 0 all, 1 no background
    // sides, 2 also only the middle of the board, a circle 0.8 of its height
    // across, where the game is played, 3 also no 3D sides on the walls. The
    // lines across the walls are never left out. Raised when a frame's lit length is
    // past LIT_BUDGET, never lowered within the level: a level that went
    // back and forth would flicker in its own way. Only with
    // Settings::simplify (off by default).
    int _lod{0};
    std::string _lodLevel;
};

namespace ui {

// Color in 0..255 with a brightness in percent.
Rgb dim(Rgb c, int percent);

void text(vp_dvg* d, const std::string& s, float x, float y, float height,
    int align, Rgb c);
float textWidth(const std::string& s, float height);

// `t` as lines no wider than `width` at `height`, broken between words and
// at line breaks.
std::vector<std::string> wrap(const std::string& t, float height, float width);
void line(vp_dvg* d, float x0, float y0, float x1, float y1);
void box(vp_dvg* d, float x0, float y0, float x1, float y1);

// The screen size setting, for the helpers above.
void setScreenSize(int percent);

} // namespace ui

} // namespace vp
