// The picture on the vector board: see render.hpp.
#include "render.hpp"

#include "SSVOpenHexagon/Core/HexagonGame.hpp"
#include "SSVOpenHexagon/Data/StyleData.hpp"
#include "SSVOpenHexagon/Global/Config.hpp"
#include "SSVOpenHexagon/Utils/FastVertexVector.hpp"

#include <vectorpie/vp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstring>
#include <sstream>
#include <vector>

namespace vp {

namespace {

// The layers, in the original's draw order (HexagonGame::draw: the 3D
// layers of walls, pivot and player first, then the walls, the cap, the
// pivot and the player).
enum Layer
{
    L_BACKGROUND,
    L_WALLS_3D,
    L_PIVOT_3D,
    L_PLAYER_3D,
    L_PARTICLES,
    L_WALLS,
    L_CAP,
    L_PIVOT,
    L_PLAYER
};

float g_screen = 0.96f; // ui::setScreenSize

// The original's 1024 x 768 screen, the background camera's view onto it.
struct ViewMap
{
    float cx, cy, w, h, c, s, k;

    // `zoom` above 1 shows more of the field (the view size setting).
    explicit ViewMap(const sf::View& v, float screen, float zoom = 1.f)
    {
        cx = v.getCenter().x;
        cy = v.getCenter().y;
        w = v.getSize().x;
        h = v.getSize().y;
        const float r = v.getRotationDeg() * 3.14159265f / 180.f;
        c = std::cos(r);
        s = std::sin(r);
        k = 4096.f * screen / zoom;
    }

    [[nodiscard]] V2 point(sf::Vector2f p) const
    {
        const float dx = p.x - cx, dy = p.y - cy;
        const float x = dx * c + dy * s;
        const float y = -dx * s + dy * c;
        return {2048.f + x / w * k, 2048.f - y / h * k};
    }

    [[nodiscard]] V2 vec(sf::Vector2f p) const
    {
        const float x = p.x * c + p.y * s;
        const float y = -p.x * s + p.y * c;
        return {x / w * k, -y / h * k};
    }
};

// A style drawn dark on light (the tutorial's: black walls on white) would
// be black lines on the tube. Such a style is drawn inverted: light on dark.
bool g_invert = false;

int luminance(const sf::Color& c)
{
    return (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
}

bool lightStyle(const hg::StyleData& st)
{
    const auto& cols = st.getColors();
    if(cols.empty()) return false;
    int sum = 0;
    for(const sf::Color& c : cols) sum += luminance(c);
    return luminance(st.getMainColor()) + 40 < sum / static_cast<int>(cols.size());
}

// vp_text in the Asteroids font, lined up on the letters themselves: the
// SDK counts every letter's advance, 8 units of letter and 4 of space (of a
// 12 unit capital), the last letter's space too. So centered text would sit
// half a space left of its middle, and right-aligned text a space short of
// its end.
void strokeText(vp_dvg* d, const char* s, float x, float y, float height, int align)
{
    vp_text_font(VP_FONT_ASTEROIDS);
    const float space = height * 4.f / 12.f;
    if(align == VP_TEXT_CENTER) x += space * 0.5f;
    else if(align == VP_TEXT_RIGHT) x += space;
    vp_text(d, s, x, y, height, align);
}

std::uint8_t lerp8(std::uint8_t a, float to, float t)
{
    return static_cast<std::uint8_t>(a + (to - a) * t);
}

// A color as the beam shows it: alpha as brightness (the original paints
// over black), the setting's brightness, and the death flash drawn over it.
Rgb beam(sf::Color c, int percent, float flash)
{
    if(g_invert)
    {
        c = sf::Color(255 - c.r, 255 - c.g, 255 - c.b, c.a);
    }

    const float k = (c.a / 255.f) * (percent / 100.f);
    Rgb r{static_cast<std::uint8_t>(std::min(255.f, c.r * k)),
        static_cast<std::uint8_t>(std::min(255.f, c.g * k)),
        static_cast<std::uint8_t>(std::min(255.f, c.b * k))};

    if(flash > 0.f)
    {
        r.r = lerp8(r.r, 255.f, flash);
        r.g = lerp8(r.g, 255.f, flash);
        r.b = lerp8(r.b, 255.f, flash);
    }

    return r;
}

// The original shows a dark shape against its colored background (a wall
// whose hue the level turned, transformHue, comes out black from a grey).
// On the tube a dark line is no line: a wall, the pivot or the player that
// would be drawn darker than LIFT_FLOOR keeps its hue at LIFT_TO, and one
// with no hue left takes the background's color, against which the
// original shows it.
constexpr int LIFT_FLOOR = 70;
constexpr int LIFT_TO = 160;

Rgb lift(Rgb c, Rgb background)
{
    const int m = std::max({c.r, c.g, c.b});
    if(m >= LIFT_FLOOR) return c;
    if(m >= 12)
    {
        const float k = static_cast<float>(LIFT_TO) / m;
        return {static_cast<std::uint8_t>(std::min(255.f, c.r * k)),
            static_cast<std::uint8_t>(std::min(255.f, c.g * k)),
            static_cast<std::uint8_t>(std::min(255.f, c.b * k))};
    }
    return background;
}

// A tone of a color: its hue turned by `deg` and its value scaled. The
// 3D band's and the fill's (VectorPie, not the original's: on the beam a
// wall, its side and its fill are lines alike, a tone tells them apart).
Rgb tone(Rgb c, float deg, float value, bool tintGrey = false)
{
    const float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f;
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    const float d = mx - mn;
    float h = 0.f;
    if(d > 1e-4f)
    {
        if(mx == r) h = std::fmod((g - b) / d, 6.f);
        else if(mx == g) h = (b - r) / d + 2.f;
        else h = (r - g) / d + 4.f;
        h *= 60.f;
        if(h < 0.f) h += 360.f;
    }
    float sat = mx > 0.f ? d / mx : 0.f;
    if(tintGrey && sat < 0.12f)
    {
        // A grey style: the band in a steel blue, faint.
        h = 210.f - deg;
        sat = 0.38f;
    }
    const float v = std::min(1.f, mx * value);
    h = std::fmod(h + deg + 360.f, 360.f);

    const float cc = v * sat, x = cc * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f)), m = v - cc;
    float rr = 0, gg = 0, bb = 0;
    if(h < 60) { rr = cc; gg = x; }
    else if(h < 120) { rr = x; gg = cc; }
    else if(h < 180) { gg = cc; bb = x; }
    else if(h < 240) { gg = x; bb = cc; }
    else if(h < 300) { rr = x; bb = cc; }
    else { rr = cc; bb = x; }
    return {static_cast<std::uint8_t>((rr + m) * 255.f), static_cast<std::uint8_t>((gg + m) * 255.f),
        static_cast<std::uint8_t>((bb + m) * 255.f)};
}

bool dark(Rgb c)
{
    return c.r < 6 && c.g < 6 && c.b < 6;
}

// The convex hull of up to 8 points (Andrew's monotone chain).
int hull(V2* p, int n, V2* out)
{
    std::sort(p, p + n, [](const V2& a, const V2& b)
        { return a.x < b.x || (a.x == b.x && a.y < b.y); });

    auto cr = [](V2 o, V2 a, V2 b)
    { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); };

    int k = 0;
    for(int i = 0; i < n; ++i)
    {
        while(k >= 2 && cr(out[k - 2], out[k - 1], p[i]) <= 0.f) --k;
        out[k++] = p[i];
    }
    for(int i = n - 2, t = k + 1; i >= 0; --i)
    {
        while(k >= t && cr(out[k - 2], out[k - 1], p[i]) <= 0.f) --k;
        out[k++] = p[i];
    }
    return k - 1;
}

bool convex(const V2* p, int n)
{
    int sign = 0;
    for(int i = 0; i < n; ++i)
    {
        const V2 a = p[i], b = p[(i + 1) % n], c = p[(i + 2) % n];
        const float z = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
        if(std::fabs(z) < 1e-3f) continue;
        const int s = z > 0 ? 1 : -1;
        if(sign && s != sign) return false;
        sign = s;
    }
    return true;
}

// A shape: its outline on `top`, and, with 3D, the band from it to `off`
// on `band` that hides what is behind.
void addShape(Occluder& occ, const V2* p, int n, Rgb color, int top,
    bool threeD, V2 off, Rgb bandColor, int band, int fill = 0, Rgb fillColor = {0, 0, 0})
{
    if(n != 4 || convex(p, n))
    {
        occ.addPolygon(p, n, top, color, true);

        // VectorPie: a wall is a filled block in the original, here an
        // outline. Lines along it, between its two long sides, make it read
        // as one: 0 off, 1 a line every 110 board units (at most 2), 2 every
        // 55 (at most 5).
        if(fill > 0 && n == 4)
        {
            auto len = [](V2 a, V2 b) { return std::hypot(b.x - a.x, b.y - a.y); };
            // The long sides: 0-1 and 3-2, or 1-2 and 0-3.
            const bool alongA = len(p[0], p[1]) + len(p[3], p[2]) >= len(p[1], p[2]) + len(p[0], p[3]);
            const V2 a0 = alongA ? p[0] : p[1], a1 = alongA ? p[1] : p[2];
            const V2 b0 = alongA ? p[3] : p[0], b1 = alongA ? p[2] : p[3];
            const float across = 0.5f * (len(a0, b0) + len(a1, b1));
            const float every = fill == 2 ? 55.f : 110.f;
            const int most = fill == 2 ? 5 : 2;
            const int lines = std::min(most, static_cast<int>(across / every));
            for(int i = 1; i <= lines; ++i)
            {
                const float t = static_cast<float>(i) / (lines + 1);
                const V2 s0{a0.x + (b0.x - a0.x) * t, a0.y + (b0.y - a0.y) * t};
                const V2 s1{a1.x + (b1.x - a1.x) * t, a1.y + (b1.y - a1.y) * t};
                occ.addLine(s0, s1, top, fillColor);
            }
        }
    }
    else
    {
        // A custom wall bent out of shape: its outline, hiding as two
        // triangles.
        V2 t1[3] = {p[0], p[1], p[2]};
        V2 t2[3] = {p[0], p[2], p[3]};
        occ.addPolygon(t1, 3, top, color, false);
        occ.addPolygon(t2, 3, top, color, false);
        for(int i = 0; i < n; ++i)
        {
            occ.addLine(p[i], p[(i + 1) % n], top, color);
        }
    }

    if(!threeD)
    {
        return;
    }

    V2 pts[8], out[10];
    for(int i = 0; i < n; ++i)
    {
        pts[i] = p[i];
        pts[n + i] = {p[i].x + off.x, p[i].y + off.y};
    }

    const int h = hull(pts, 2 * n, out);
    occ.addPolygon(out, h, band, bandColor, !dark(bandColor));
}

void spriteShape(Occluder& occ, V2 c, float r, int sides, float rot, Rgb color)
{
    V2 p[8];
    sides = std::min(sides, 8);
    for(int i = 0; i < sides; ++i)
    {
        const float a = rot + 6.2831853f * i / sides;
        p[i] = {c.x + std::cos(a) * r, c.y + std::sin(a) * r};
    }
    for(int i = 0; i < sides; ++i)
    {
        occ.addLine(p[i], p[(i + 1) % sides], L_PARTICLES, color);
    }
}

} // namespace

namespace {

// Levels drawn slimmer than the rest, by hand: too many lines for the board
// otherwise (2026-10-18). Arcadia: its bars over the whole height, about
// 205000 lit on average, peaks 420000; slimmed about 50000 / 92000.
struct LevelLook
{
    const char* idEnd;  // the end of the level's id
    bool spokes;        // background sides
    bool wall3D;        // 3D sides of the walls
    bool thinAsLine;    // a thin wall as one line along its middle
};

const LevelLook LOOKS[] = {
    {"_orthoplex_1_arcadia", false, false, true},
};

const LevelLook* lookFor(const std::string& id)
{
    for(const LevelLook& l : LOOKS)
    {
        const std::size_t n = std::strlen(l.idEnd);
        if(id.size() >= n && id.compare(id.size() - n, n, l.idEnd) == 0) return &l;
    }
    return nullptr;
}

// A wall much longer than wide: its middle line, from end to end.
bool thinWall(const V2* p, V2& a, V2& b)
{
    auto len = [](V2 u, V2 v) { return std::hypot(v.x - u.x, v.y - u.y); };
    const float s01 = len(p[0], p[1]) + len(p[3], p[2]);
    const float s12 = len(p[1], p[2]) + len(p[0], p[3]);
    const bool alongA = s01 >= s12;
    const float along = std::max(s01, s12) * 0.5f, across = std::min(s01, s12) * 0.5f;
    if(across * 3.f > along) return false;
    auto mid = [](V2 u, V2 v) { return V2{(u.x + v.x) * 0.5f, (u.y + v.y) * 0.5f}; };
    a = alongA ? mid(p[0], p[3]) : mid(p[0], p[1]);
    b = alongA ? mid(p[1], p[2]) : mid(p[3], p[2]);
    return true;
}

} // namespace

void Renderer::drawGame(hg::HexagonGame& g, vp_dvg* d, const Settings& s, bool hud)
{
    using hg::Config::get3D;

    g_screen = s.screenSize / 100.f;
    _occ.clear();

    g_invert = lightStyle(g.styleData);
    // Simpler drawing only when the service menu says so (2026-10-18:
    // the user wants to see every level in full first).
    if(g.getLevelId() != _lodLevel || !s.simplify)
    {
        _lodLevel = g.getLevelId();
        _lod = 0;
    }

    const ViewMap view(g.backgroundCamera->apply(), g_screen, s.viewSize / 100.f);
    const float flash = hg::Config::getFlash() ? g.status.flashEffect / 255.f : 0.f;

    // ------------------------------------------------------------------
    // Background: the sides as spokes between sectors of different color
    // (StyleData::drawBackgroundImpl: center, angle + half, angle - half).
    // The simplest step: only the middle of the board (see render.hpp).
    _occ.setClipRadius(_lod >= 2 ? 4096.f * g_screen * 0.40f : 0.f);

    const LevelLook* look = lookFor(g.getLevelId());

    if(s.spokes && (!look || look->spokes) && _lod < 1 && g.backgroundTris.size() >= 3)
    {
        const std::size_t n = g.backgroundTris.size() / 3;
        for(std::size_t i = 0; i < n; ++i)
        {
            const sf::Vertex& c = g.backgroundTris[i * 3];
            const sf::Vertex& p1 = g.backgroundTris[i * 3 + 1];
            const sf::Vertex& next = g.backgroundTris[((i + 1) % n) * 3];

            if(c.color == next.color && n > 1)
            {
                continue;
            }

            sf::Color col{std::max(c.color.r, next.color.r),
                std::max(c.color.g, next.color.g),
                std::max(c.color.b, next.color.b),
                std::max(c.color.a, next.color.a)};

            const Rgb rgb = beam(col, s.brightness[BRIGHT_BACKGROUND], flash);
            if(!dark(rgb))
            {
                _occ.addLine(view.point(c.position), view.point(p1.position),
                    L_BACKGROUND, rgb);
            }
        }
    }

    // The color a hueless dark shape is lifted to: the background's, at
    // LIFT_TO (grey when the background is dark too).
    Rgb liftBg{LIFT_TO, LIFT_TO, LIFT_TO};
    {
        const auto& cols = g.styleData.getColors();
        if(!cols.empty())
        {
            int r = 0, gg = 0, b = 0;
            for(const sf::Color& c : cols)
            {
                const Rgb x = beam(sf::Color(c.r, c.g, c.b, 255), 100, 0.f);
                r += x.r; gg += x.g; b += x.b;
            }
            const int n = static_cast<int>(cols.size());
            const int m = std::max({r / n, gg / n, b / n});
            if(m >= 12)
            {
                liftBg = {static_cast<std::uint8_t>(r / n * LIFT_TO / m),
                    static_cast<std::uint8_t>(gg / n * LIFT_TO / m),
                    static_cast<std::uint8_t>(b / n * LIFT_TO / m)};
            }
        }
    }

    const auto liftAt = [&](Rgb c, int percent)
    { return ui::dim(lift(c, liftBg), percent); };

    const bool threeD = get3D() && !g.vpLayers3D.empty();

    // The lines across a wall are never left out: without them a wide wall
    // is an empty outline, not seen as a wall (Pointless, 2026-10-18).
    const bool wall3D = threeD && _lod < 3 && (!look || look->wall3D);
    const int wallFill = s.wallFill;
    // The original draws `depth` layers, each darker the deeper it is; the
    // band takes the color of the nearest one.
    const auto* near = g.vpLayers3D.empty() ? nullptr : &g.vpLayers3D.back();
    const V2 off = threeD ? view.vec(g.vpLayers3D.front().offset) : V2{0.f, 0.f};

    // ------------------------------------------------------------------
    // Walls (CWall::draw and CCustomWallManager::draw: quads as
    // nw, sw, se / nw, se, ne).
    {
        const auto& q = g.wallQuads;
        const std::size_t n = q.size() / 6;
        for(std::size_t i = 0; i < n; ++i)
        {
            const sf::Vertex* v = &q[i * 6];
            const V2 p[4] = {view.point(v[0].position), view.point(v[1].position),
                view.point(v[2].position), view.point(v[5].position)};

            const Rgb top = liftAt(beam(v[0].color, 100, flash), s.brightness[BRIGHT_WALLS]);
            Rgb band = wall3D ? liftAt(beam(near->wallColor, 100, flash), s.brightness[BRIGHT_3D]) : Rgb{0, 0, 0};
            Rgb fillColor = ui::dim(top, 70);
            if(s.tints)
            {
                // The band from the wall's own color, not the original's
                // darkened 3D color: on the tube a side darker than that is
                // not seen at all.
                if(wall3D) band = ui::dim(tone(liftAt(beam(v[0].color, 100, flash), 100), 35.f, 0.8f, true), s.brightness[BRIGHT_3D]);
                fillColor = tone(top, -18.f, 0.7f, true);
            }

            V2 a, b;
            if(look && look->thinAsLine && thinWall(p, a, b))
            {
                // Hides what is behind it as the whole wall, drawn as its
                // middle line.
                _occ.addPolygon(p, 4, L_WALLS, top, false);
                _occ.addLine(a, b, L_WALLS, top);
                continue;
            }
            addShape(_occ, p, 4, top, L_WALLS, wall3D, off, band, L_WALLS_3D, wallFill, fillColor);
        }
    }

    // ------------------------------------------------------------------
    // The cap (CPlayer::drawPivot: center triangles).
    {
        const auto& t = g.capTris;
        const std::size_t n = t.size() / 3;
        for(std::size_t i = 0; i < n; ++i)
        {
            const sf::Vertex* v = &t[i * 3];
            const V2 p[3] = {view.point(v[0].position), view.point(v[1].position),
                view.point(v[2].position)};
            _occ.addPolygon(p, 3, L_CAP, beam(v[0].color, s.brightness[BRIGHT_WALLS], flash), true);
        }
    }

    // ------------------------------------------------------------------
    // The pivot and the death effect (CPlayer::drawPivot, drawDeathEffect).
    {
        const auto& q = g.pivotQuads;
        const std::size_t n = q.size() / 6;
        for(std::size_t i = 0; i < n; ++i)
        {
            const sf::Vertex* v = &q[i * 6];
            const V2 p[4] = {view.point(v[0].position), view.point(v[1].position),
                view.point(v[2].position), view.point(v[5].position)};

            const Rgb top = liftAt(beam(v[0].color, 100, flash), s.brightness[BRIGHT_WALLS]);
            Rgb band = threeD ? liftAt(beam(near->pivotColor, 100, flash), s.brightness[BRIGHT_3D]) : Rgb{0, 0, 0};
            if(s.tints && threeD) band = ui::dim(tone(liftAt(beam(v[0].color, 100, flash), 100), 35.f, 0.8f, true), s.brightness[BRIGHT_3D]);
            addShape(_occ, p, 4, top, L_PIVOT, threeD, off, band, L_PIVOT_3D);
        }
    }

    // ------------------------------------------------------------------
    // The player (CPlayer::draw: one triangle).
    {
        const auto& t = g.playerTris;
        const std::size_t n = t.size() / 3;
        for(std::size_t i = 0; i < n; ++i)
        {
            const sf::Vertex* v = &t[i * 3];
            const V2 p[3] = {view.point(v[0].position), view.point(v[1].position),
                view.point(v[2].position)};

            const Rgb top = liftAt(beam(v[0].color, 100, flash), s.brightness[BRIGHT_PLAYER]);
            Rgb band = threeD ? liftAt(beam(near->playerColor, 100, flash), s.brightness[BRIGHT_3D]) : Rgb{0, 0, 0};
            if(s.tints && threeD) band = ui::dim(tone(liftAt(beam(v[0].color, 100, flash), 100), 35.f, 0.8f, true), s.brightness[BRIGHT_3D]);
            addShape(_occ, p, 3, top, L_PLAYER, threeD, off, band, L_PLAYER_3D);
        }
    }

    // ------------------------------------------------------------------
    // Trail and swap particles (smallCircle.png, 16 px): small hexagons.
    // VectorPie: the trail's alpha (35 in the original) is raised four
    // times, or a line that thin would not show on the tube at all.
    if(s.trail)
    {
        const float pxToBoard = view.k / view.w;

        for(const auto& p : g.trailParticles)
        {
            sf::Color c = p.sprite.getColor();
            c.a = static_cast<std::uint8_t>(std::min(255, c.a * 4));
            const Rgb rgb = beam(c, s.brightness[BRIGHT_PLAYER], flash);
            if(dark(rgb)) continue;
            spriteShape(_occ, view.point(p.sprite.getPosition()),
                8.f * p.sprite.getScale().x * pxToBoard, 6, 0.f, rgb);
        }

        for(const auto& p : g.swapParticles)
        {
            sf::Color c = p.sprite.getColor();
            c.a = static_cast<std::uint8_t>(std::min(255, c.a * 2));
            const Rgb rgb = beam(c, s.brightness[BRIGHT_PLAYER], flash);
            if(dark(rgb)) continue;
            spriteShape(_occ, view.point(p.sprite.getPosition()),
                8.f * p.sprite.getScale().x * pxToBoard, 6, 0.f, rgb);
        }
    }

    _occ.draw(d);
    _stats = _occ.stats();

    // Too long for the board: draw this level simpler from the next frame.
    constexpr float LIT_BUDGET = 110000.f;
    if(s.simplify && _stats.lit > LIT_BUDGET && _lod < 3 && !g.hasDied())
    {
        ++_lod;
        std::fprintf(stderr, "openhexagon: %s draws %.0f lit, simpler from now (%d)\n",
            _lodLevel.c_str(), _stats.lit, _lod);
    }

    // ------------------------------------------------------------------
    // The overlay (overlayCamera: the 1024 x 768 screen, shaken with the
    // background on a death).
    const ViewMap over(g.overlayCamera->apply(), g_screen);

    // Personal best stars (starParticle.png, 64 px): five-pointed outlines.
    for(const auto& p : g.particles)
    {
        const Rgb rgb = beam(p.sprite.getColor(), s.brightness[BRIGHT_TEXT], 0.f);
        const V2 c = over.point(p.sprite.getPosition());
        const float r = 28.f * p.sprite.getScale().x * over.k / over.w;
        const float rot = p.sprite.getRotation().asDegrees() * 3.14159265f / 180.f;
        vp_color(d, rgb.r, rgb.g, rgb.b);
        for(int i = 0; i <= 5; ++i)
        {
            const float a = rot + 6.2831853f * ((i * 2) % 5) / 5.f;
            const int x = static_cast<int>(c.x + std::cos(a) * r);
            const int y = static_cast<int>(c.y + std::sin(a) * r);
            if(i == 0) vp_moveto(d, x, y);
            else vp_lineto(d, x, y);
        }
    }

    if(!hud)
    {
        return;
    }

    // Text (drawText_TimeAndStatus, drawText_Message, drawText_PersonalBest),
    // at the original's places and sizes on its 1024 x 768 screen.
    const Rgb textColor = beam(g.getColorText(), s.brightness[BRIGHT_TEXT], 0.f);
    const float px = over.k / 768.f;       // board units per pixel, vertically
    const float cap = 0.72f;               // a capital's height per character size

    vp_text_font(VP_FONT_ASTEROIDS);
    vp_color(d, textColor.r, textColor.g, textColor.b);

    auto at = [&](float x, float y) { return over.point({x, y}); };

    // VectorPie: the stroke font is wider than the original's OpenSquare; a
    // line that would leave the screen is made smaller until it fits.
    const float maxWidth = over.k * 0.94f;
    auto fit = [&](const std::string& t, float height) {
        const float w = vp_text_width(t.c_str(), height);
        return w > maxWidth ? height * maxWidth / w : height;
    };

    const float padding = 8.f;
    {
        const float h = 70.f * cap;
        const V2 p = at(padding, padding + h);
        vp_text(d, g.timeText.getString().c_str(), p.x, p.y, h * px, VP_TEXT_LEFT);
    }

    // Where the status lines end: the message goes below them, they
    // overlapped in the tutorials (2026-10-19).
    float statusEnd = padding + 70.f + padding;
    {
        std::istringstream lines(g.text.getString());
        std::string l;
        float y = padding + 70.f + padding;
        const float h = 20.f * cap;
        while(std::getline(lines, l))
        {
            y += 24.f;
            if(l.empty()) continue;
            const V2 p = at(padding, y);
            vp_text(d, l.c_str(), p.x, p.y, fit(l, h * px), VP_TEXT_LEFT);
            statusEnd = y;
        }
    }

    if(!g.messageText.getString().isEmpty())
    {
        std::istringstream lines(g.messageText.getString());
        std::string l;
        const float h = 32.f * cap;
        // The first line's top 12 px below the status lines.
        float y = std::max(768.f / 5.5f, statusEnd + 12.f + h - 38.f);
        while(std::getline(lines, l))
        {
            // A line too wide for the screen goes on in the next line (the
            // original's font is narrower), word by word.
            std::vector<std::string> parts;
            std::string part, word;
            std::istringstream words(l);
            // A little too wide: smaller, as before, not one word alone in
            // a line of its own.
            const bool nearlyFits = vp_text_width(l.c_str(), h * px) <= maxWidth * 1.15f;
            if(nearlyFits) part = l;
            while(!nearlyFits && words >> word)
            {
                const std::string t = part.empty() ? word : part + ' ' + word;
                if(!part.empty() && vp_text_width(t.c_str(), h * px) > maxWidth) { parts.push_back(part); part = word; }
                else part = t;
            }
            parts.push_back(part);
            for(const std::string& one : parts)
            {
                y += 38.f;
                const V2 p = at(512.f, y);
                strokeText(d, one.c_str(), p.x, p.y, fit(one, h * px), VP_TEXT_CENTER);
            }
        }
    }

    if(!g.pbText.getString().isEmpty())
    {
        const float h = static_cast<float>(g.pbText.getCharacterSize()) * cap * 0.8f;
        const V2 p = at(512.f, 768.f - 768.f / 4.f + h);
        strokeText(d, g.pbText.getString().c_str(), p.x, p.y, fit(g.pbText.getString(), h * px), VP_TEXT_CENTER);
    }

    if(!g.replayText.getString().isEmpty())
    {
        const float h = 16.f * cap;
        const V2 p = at(1024.f - padding, padding + h);
        strokeText(d, g.replayText.getString().c_str(), p.x, p.y, h * px, VP_TEXT_RIGHT);
    }
}

int Renderer::mostlyHiddenWalls() const
{
    int n = 0;
    for(int i = 0; i < _occ.polygonCount(); ++i)
    {
        if(_occ.polygonLayer(i) != L_WALLS || !_occ.polygonOnBoard(i)) continue;
        const float f = _occ.visibleFraction(i);
        if(f >= 0.f && f < 0.33f) ++n;
    }
    return n;
}

Renderer::Probe Renderer::probe(hg::HexagonGame& g, const Settings& s)
{
    g_invert = lightStyle(g.styleData);
    Probe p{-1, g_invert, 0};
    const auto& q = g.wallQuads;
    const std::size_t n = q.size() / 6;
    p.walls = static_cast<int>(n);
    for(std::size_t i = 0; i < n; ++i)
    {
        const Rgb c = beam(q[i * 6].color, s.brightness[BRIGHT_WALLS], 0.f);
        const int l = std::max({c.r, c.g, c.b});
        if(p.darkestWall < 0 || l < p.darkestWall) p.darkestWall = l;
    }
    return p;
}

void Renderer::drawMenuBackground(const hg::StyleData& style,
    unsigned int sides, float rotationDeg, vp_dvg* d, const Settings& s)
{
    g_screen = s.screenSize / 100.f;
    _occ.clear();

    g_invert = lightStyle(style);

    sf::View v{{0.f, 0.f}, {1024.f, 768.f}};
    v.setRotation(rotationDeg);
    const ViewMap view(v, g_screen, s.viewSize / 100.f);

    hg::Utils::FastVertexVectorTris tris;
    style.drawBackgroundMenu(tris, {0.f, 0.f}, sides, true, false, true);

    const std::size_t bg = sides;
    const std::size_t n = tris.size() / 3;

    for(std::size_t i = 0; i < n; ++i)
    {
        const sf::Vertex* t = &tris[i * 3];

        if(i < bg)
        {
            if(!s.spokes) continue;
            const sf::Vertex& next = tris[((i + 1) % bg) * 3];
            if(t[0].color == next.color) continue;
            sf::Color col{std::max(t[0].color.r, next.color.r),
                std::max(t[0].color.g, next.color.g),
                std::max(t[0].color.b, next.color.b), 255};
            _occ.addLine(view.point(t[0].position), view.point(t[1].position),
                L_BACKGROUND, beam(col, s.brightness[BRIGHT_BACKGROUND], 0.f));
            continue;
        }

        // drawBackgroundMenuHexagonImpl: the main color's hexagon, then the
        // cap's over it.
        const bool capTri = ((i - bg) % 2) == 1;
        const V2 p[3] = {view.point(t[0].position), view.point(t[1].position),
            view.point(t[2].position)};
        _occ.addPolygon(p, 3, capTri ? L_CAP : L_WALLS,
            beam(t[0].color, s.brightness[BRIGHT_WALLS], 0.f), true);
    }

    _occ.draw(d);
    _stats = _occ.stats();
}

// ---------------------------------------------------------------------------
// The title screen (VectorPie, not the original's: its title is a picture).
// After the marquee: "OPEN" in gold to orange over "HEXAGON" in white, in
// the original's logo font (OpenSquare Bold, tools/make_logo_font.py), as
// solid letters with a red to magenta side, over a turning tunnel of walls
// from orange through red to magenta. Nothing pulses (the user, 2026-10-18).

namespace {

struct LogoGlyph
{
    char c;
    float advance;          // in capital heights
    const float* contours;  // n, x0, y0, ... per contour, 0 at the end
    const float* tris;      // x, y three times per triangle
    int ntris;
};

#include "gen/logo_font.inc"

const LogoGlyph* logoGlyph(char c)
{
    static const LogoGlyph* const all[] = {&LOGO_O, &LOGO_P, &LOGO_E, &LOGO_N, &LOGO_H, &LOGO_X, &LOGO_A, &LOGO_G};
    for(const LogoGlyph* g : all)
        if(g->c == c) return g;
    return nullptr;
}

constexpr int L_TITLE_3D = 20;
constexpr int L_TITLE = 21;

Rgb lerpRgb(Rgb a, Rgb b, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return {static_cast<std::uint8_t>(a.r + (b.r - a.r) * t), static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<std::uint8_t>(a.b + (b.b - a.b) * t)};
}

// A word in the logo font, centered on (cx, cy) (board units), a capital
// `cap` board units high. Faces from `face0` to `face1` letter by letter,
// the sides from `side0` to `side1`, `depth` deep. The letters' triangles
// hide what is behind them, the band from each triangle to its far copy
// is the side; the outline is the font's own contours.
void titleWord(Occluder& occ, const char* word, float cx, float cy, float cap,
    Rgb face0, Rgb face1, Rgb side0, Rgb side1, V2 depth)
{
    const int n = static_cast<int>(std::strlen(word));
    float total = 0.f;
    for(int i = 0; i < n; ++i)
        if(const LogoGlyph* g = logoGlyph(word[i])) total += g->advance;

    float x = cx - total * cap * 0.5f;
    const float y0 = cy - cap * 0.5f;
    for(int i = 0; i < n; ++i)
    {
        const LogoGlyph* g = logoGlyph(word[i]);
        if(g == nullptr) continue;
        const float t = n > 1 ? static_cast<float>(i) / (n - 1) : 0.f;
        const Rgb face = lerpRgb(face0, face1, t);
        const Rgb side = lerpRgb(side0, side1, t);
        auto at = [&](float u, float v) { return V2{x + u * cap, y0 + v * cap}; };

        for(int k = 0; k < g->ntris; ++k)
        {
            const float* q = g->tris + k * 6;
            const V2 tri[3] = {at(q[0], q[1]), at(q[2], q[3]), at(q[4], q[5])};
            occ.addPolygon(tri, 3, L_TITLE, face, false);

            V2 pts[6], out[8];
            for(int m = 0; m < 3; ++m)
            {
                pts[m] = tri[m];
                pts[3 + m] = {tri[m].x + depth.x, tri[m].y + depth.y};
            }
            const int h = hull(pts, 6, out);
            occ.addPolygon(out, h, L_TITLE_3D, side, true);
        }

        for(const float* c = g->contours; *c > 0.f;)
        {
            const int m = static_cast<int>(*c++);
            for(int k = 0; k < m; ++k)
            {
                const int k2 = (k + 1) % m;
                occ.addLine(at(c[k * 2], c[k * 2 + 1]), at(c[k2 * 2], c[k2 * 2 + 1]), L_TITLE, face);
            }
            c += m * 2;
        }

        x += g->advance * cap;
    }
}

} // namespace

void Renderer::drawTitle(double time, float beatHz, vp_dvg* d, const Settings& s)
{
    (void)beatHz; // the title does not pulse
    g_screen = s.screenSize / 100.f;
    g_invert = false;
    _occ.clear();

    const float t = static_cast<float>(time);

    // --- the tunnel: the original's look, its own walls
    sf::View v{{0.f, 0.f}, {1024.f, 768.f * 1.18f}};
    v.setRotation(t * 14.f);
    const ViewMap view(v, g_screen);

    const Rgb ORANGE{255, 130, 20}, RED{235, 32, 64}, MAGENTA{200, 30, 150}, GOLD{255, 214, 60};
    auto sideColor = [&](int side)
    {
        // Around the hexagon: orange at the top, red, magenta at the bottom.
        const float a = std::fabs(std::fmod(side / 6.f + 1.f, 1.f) - 0.5f) * 2.f; // 1 at side 0, 0 at side 3
        return a > 0.5f ? lerpRgb(RED, ORANGE, (a - 0.5f) * 2.f) : lerpRgb(MAGENTA, RED, a * 2.f);
    };

    const float radius = 70.f;
    const float div = 6.2831853f / 6.f;
    const V2 off = view.vec({0.f, 26.f});

    // Background sides.
    if(s.spokes)
    {
        for(int i = 0; i < 6; ++i)
        {
            const float a = div * i + div * 0.5f;
            _occ.addLine(view.point({std::cos(a) * radius, std::sin(a) * radius}),
                view.point({std::cos(a) * 1600.f, std::sin(a) * 1600.f}), L_BACKGROUND,
                ui::dim(Rgb{120, 30, 60}, s.brightness[BRIGHT_BACKGROUND]));
        }
    }

    // Rings of walls coming in, a pattern per ring.
    const float speed = 190.f, spacing = 230.f;
    for(int ring = 0; ring < 9; ++ring)
    {
        const float travel = std::fmod(t * speed, spacing);
        const float r = radius + 30.f + ring * spacing - travel;
        if(r < radius + 6.f) continue;
        const int id = static_cast<int>(std::floor(t * speed / spacing)) + ring;
        const unsigned pattern = (static_cast<unsigned>(id) * 2654435761u) >> 26; // 6 bits
        for(int side = 0; side < 6; ++side)
        {
            if(((pattern >> side) & 1u) == 0u && ((id + side) % 3 != 0)) continue; // a gap
            const float a0 = div * side, a1 = div * (side + 1);
            const float thick = 34.f;
            const sf::Vector2f q[4] = {{std::cos(a0) * r, std::sin(a0) * r}, {std::cos(a1) * r, std::sin(a1) * r},
                {std::cos(a1) * (r + thick), std::sin(a1) * (r + thick)}, {std::cos(a0) * (r + thick), std::sin(a0) * (r + thick)}};
            V2 p[4];
            for(int k = 0; k < 4; ++k) p[k] = view.point(q[k]);
            const Rgb top = ui::dim(sideColor(side), s.brightness[BRIGHT_WALLS] * 70 / 100);
            const Rgb band = ui::dim(tone(sideColor(side), 35.f, 0.6f), s.brightness[BRIGHT_3D] * 70 / 100);
            addShape(_occ, p, 4, top, L_WALLS, true, off, band, L_WALLS_3D, s.wallFill, tone(top, -18.f, 0.7f));
        }
    }

    // The center hexagon.
    {
        V2 p[6];
        for(int i = 0; i < 6; ++i)
        {
            const float a = div * i;
            p[i] = view.point({std::cos(a) * radius * 0.75f, std::sin(a) * radius * 0.75f});
        }
        for(int i = 0; i < 6; ++i)
        {
            const V2 tri[3] = {view.point({0.f, 0.f}), p[i], p[(i + 1) % 6]};
            _occ.addPolygon(tri, 3, L_CAP, ui::dim(RED, s.brightness[BRIGHT_WALLS]), true);
        }
    }

    // --- the logo: still
    const float k = 4096.f * g_screen;
    const float cap = k * 0.105f;              // HEXAGON's capitals
    const V2 depth{cap * 0.07f, -cap * 0.16f};  // the side: down and a little right
    const int tb = s.brightness[BRIGHT_TEXT];
    titleWord(_occ, "OPEN", 2048.f, 2048.f + cap * 1.18f, cap * 0.6f,
        ui::dim(GOLD, tb), ui::dim(ORANGE, tb), ui::dim(Rgb{170, 60, 10}, tb), ui::dim(Rgb{170, 34, 16}, tb),
        {depth.x * 0.6f, depth.y * 0.6f});
    titleWord(_occ, "HEXAGON", 2048.f, 2048.f, cap,
        ui::dim(Rgb{255, 255, 255}, tb), ui::dim(Rgb{255, 240, 245}, tb), ui::dim(Rgb{200, 24, 52}, tb),
        ui::dim(Rgb{170, 24, 120}, tb), depth);

    _occ.draw(d);
    _stats = _occ.stats();
}

namespace ui {

Rgb dim(Rgb c, int percent)
{
    return {static_cast<std::uint8_t>(c.r * percent / 100),
        static_cast<std::uint8_t>(c.g * percent / 100),
        static_cast<std::uint8_t>(c.b * percent / 100)};
}

void setScreenSize(int percent)
{
    g_screen = percent / 100.f;
}

namespace {

V2 board(float x, float y)
{
    const float k = 4096.f * g_screen;
    return {2048.f + (x - 192.f) / 384.f * k, 2048.f - (y - 144.f) / 288.f * k};
}

} // namespace

void text(vp_dvg* d, const std::string& s, float x, float y, float height,
    int align, Rgb c)
{
    const V2 p = board(x, y + height);
    vp_color(d, c.r, c.g, c.b);
    vp_text_font(VP_FONT_ASTEROIDS);
    strokeText(d, s.c_str(), p.x, p.y, height / 288.f * 4096.f * g_screen, align);
}

float textWidth(const std::string& s, float height)
{
    vp_text_font(VP_FONT_ASTEROIDS);
    const float h = height / 288.f * 4096.f * g_screen;
    return vp_text_width(s.c_str(), h) / (4096.f * g_screen) * 384.f;
}

std::vector<std::string> wrap(const std::string& t, float height, float width)
{
    std::vector<std::string> out;
    std::string line, word;
    auto flush = [&]
    {
        if(word.empty()) return;
        const std::string tryLine = line.empty() ? word : line + ' ' + word;
        if(!line.empty() && ui::textWidth(tryLine, height) > width) { out.push_back(line); line = word; }
        else line = tryLine;
        word.clear();
    };
    for(char c : t)
    {
        if(c == ' ' || c == '\n') { flush(); if(c == '\n' && !line.empty()) { out.push_back(line); line.clear(); } }
        else word += c;
    }
    flush();
    if(!line.empty()) out.push_back(line);
    return out;
}

void line(vp_dvg* d, float x0, float y0, float x1, float y1)
{
    const V2 a = board(x0, y0), b = board(x1, y1);
    vp_moveto(d, static_cast<int>(a.x), static_cast<int>(a.y));
    vp_lineto(d, static_cast<int>(b.x), static_cast<int>(b.y));
}

void box(vp_dvg* d, float x0, float y0, float x1, float y1)
{
    const V2 a = board(x0, y0), b = board(x1, y1);
    vp_moveto(d, static_cast<int>(a.x), static_cast<int>(a.y));
    vp_lineto(d, static_cast<int>(b.x), static_cast<int>(a.y));
    vp_lineto(d, static_cast<int>(b.x), static_cast<int>(b.y));
    vp_lineto(d, static_cast<int>(a.x), static_cast<int>(b.y));
    vp_lineto(d, static_cast<int>(a.x), static_cast<int>(a.y));
}

} // namespace ui

} // namespace vp
