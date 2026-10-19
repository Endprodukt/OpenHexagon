// Hidden lines in the plane: see occlude.hpp.
#include "occlude.hpp"

#include <vectorpie/vp.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vp {

namespace {

// How far inside a shape's edge a line must be to count as covered: the
// width of a seam. Lines on a shape's border count as covered, so an edge
// two shapes of one region share disappears.
constexpr float SEAM = 0.75f;

constexpr float BOARD = 4095.f;

float cross(V2 o, V2 a, V2 b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Liang-Barsky: the part of a..b on the board. False if none.
bool clipToBoard(V2& a, V2& b)
{
    float t0 = 0.f, t1 = 1.f;
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float p[4] = {-dx, dx, -dy, dy};
    const float q[4] = {a.x, BOARD - a.x, a.y, BOARD - a.y};

    for(int i = 0; i < 4; ++i)
    {
        if(p[i] == 0.f)
        {
            if(q[i] < 0.f) return false;
            continue;
        }

        const float t = q[i] / p[i];
        if(p[i] < 0.f)
        {
            if(t > t1) return false;
            if(t > t0) t0 = t;
        }
        else
        {
            if(t < t0) return false;
            if(t < t1) t1 = t;
        }
    }

    const V2 a0 = a;
    a = {a0.x + t0 * dx, a0.y + t0 * dy};
    b = {a0.x + t1 * dx, a0.y + t1 * dy};
    return true;
}

struct Interval
{
    float t0, t1;
};

// Remove [h0, h1] from the visible intervals.
void subtract(std::vector<Interval>& vis, float h0, float h1)
{
    std::size_t n = vis.size();
    for(std::size_t i = 0; i < n; ++i)
    {
        Interval v = vis[i];
        if(h1 <= v.t0 || h0 >= v.t1)
        {
            continue;
        }

        if(h0 <= v.t0 && h1 >= v.t1)
        {
            vis[i] = vis[n - 1];
            vis.pop_back();
            --n;
            --i;
            continue;
        }

        if(h0 > v.t0 && h1 < v.t1)
        {
            vis[i].t1 = h0;
            vis.push_back({h1, v.t1});
            continue;
        }

        if(h0 <= v.t0)
        {
            vis[i].t0 = h1;
        }
        else
        {
            vis[i].t1 = h0;
        }
    }
}

} // namespace

void Occluder::clear()
{
    _pts.clear();
    _polys.clear();
    _segs.clear();
    _stats = {};
}

void Occluder::addPolygon(const V2* pts, int n, int layer, Rgb color, bool outline)
{
    if(n < 3)
    {
        return;
    }

    // Anticlockwise, by the signed area.
    float area = 0.f;
    for(int i = 0; i < n; ++i)
    {
        const V2 a = pts[i], b = pts[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
    }

    if(std::fabs(area) < 1.f)
    {
        return; // a sliver: nothing to hide, nothing to draw
    }

    Poly p;
    p.first = static_cast<int>(_pts.size());
    p.n = n;
    p.layer = layer;
    p.color = color;
    p.minx = p.miny = 1e30f;
    p.maxx = p.maxy = -1e30f;

    for(int i = 0; i < n; ++i)
    {
        const V2 v = area > 0.f ? pts[i] : pts[n - 1 - i];
        _pts.push_back(v);
        p.minx = std::min(p.minx, v.x);
        p.miny = std::min(p.miny, v.y);
        p.maxx = std::max(p.maxx, v.x);
        p.maxy = std::max(p.maxy, v.y);
    }

    const int index = static_cast<int>(_polys.size());
    _polys.push_back(p);

    if(!outline)
    {
        return;
    }

    for(int i = 0; i < n; ++i)
    {
        _segs.push_back(
            Seg{pts[i], pts[(i + 1) % n], layer, index, color});
    }
}

void Occluder::addLine(V2 a, V2 b, int layer, Rgb color)
{
    _segs.push_back(Seg{a, b, layer, -1, color});
}

void Occluder::buildGrid()
{
    for(auto& c : _cells)
    {
        c.clear();
    }

    const float cell = (BOARD + 1.f) / G;

    for(int i = 0; i < static_cast<int>(_polys.size()); ++i)
    {
        const Poly& p = _polys[i];
        if(p.maxx < 0.f || p.maxy < 0.f || p.minx > BOARD || p.miny > BOARD)
        {
            continue;
        }

        const int x0 = std::clamp(static_cast<int>(p.minx / cell), 0, G - 1);
        const int x1 = std::clamp(static_cast<int>(p.maxx / cell), 0, G - 1);
        const int y0 = std::clamp(static_cast<int>(p.miny / cell), 0, G - 1);
        const int y1 = std::clamp(static_cast<int>(p.maxy / cell), 0, G - 1);

        for(int y = y0; y <= y1; ++y)
        {
            for(int x = x0; x <= x1; ++x)
            {
                _cells[y * G + x].push_back(i);
            }
        }
    }
}

void Occluder::draw(vp_dvg* d)
{
    buildGrid();

    _stats.polygons = static_cast<int>(_polys.size());
    _stats.segments = static_cast<int>(_segs.size());

    std::vector<int> stamp(_polys.size(), -1);
    std::vector<Interval> vis;
    vis.reserve(16);

    const float cell = (BOARD + 1.f) / G;

    _pieces.clear();
    _outlineLen.assign(_polys.size(), 0.f);
    _visibleLen.assign(_polys.size(), 0.f);

    for(int si = 0; si < static_cast<int>(_segs.size()); ++si)
    {
        Seg s = _segs[si];

        if(!clipToBoard(s.a, s.b))
        {
            continue;
        }

        if(_clipRadius > 0.f)
        {
            // The part of a..b within the circle round the middle.
            const float ex = s.b.x - s.a.x, ey = s.b.y - s.a.y;
            const float fx = s.a.x - 2048.f, fy = s.a.y - 2048.f;
            const float A = ex * ex + ey * ey;
            const float B = 2.f * (fx * ex + fy * ey);
            const float C = fx * fx + fy * fy - _clipRadius * _clipRadius;
            const float disc = B * B - 4.f * A * C;
            if(A < 1e-6f || disc <= 0.f) continue;
            const float sq = std::sqrt(disc);
            const float t0 = std::max(0.f, (-B - sq) / (2.f * A));
            const float t1 = std::min(1.f, (-B + sq) / (2.f * A));
            if(t1 <= t0) continue;
            const V2 a0 = s.a;
            s.a = {a0.x + ex * t0, a0.y + ey * t0};
            s.b = {a0.x + ex * t1, a0.y + ey * t1};
        }

        const float dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if(len < 0.5f)
        {
            continue;
        }

        vis.clear();
        vis.push_back({0.f, 1.f});
        if(s.owner >= 0) _outlineLen[s.owner] += len;

        const int x0 = std::clamp(static_cast<int>(std::min(s.a.x, s.b.x) / cell), 0, G - 1);
        const int x1 = std::clamp(static_cast<int>(std::max(s.a.x, s.b.x) / cell), 0, G - 1);
        const int y0 = std::clamp(static_cast<int>(std::min(s.a.y, s.b.y) / cell), 0, G - 1);
        const int y1 = std::clamp(static_cast<int>(std::max(s.a.y, s.b.y) / cell), 0, G - 1);

        const float sminx = std::min(s.a.x, s.b.x), smaxx = std::max(s.a.x, s.b.x);
        const float sminy = std::min(s.a.y, s.b.y), smaxy = std::max(s.a.y, s.b.y);

        for(int cy = y0; cy <= y1 && !vis.empty(); ++cy)
        {
            for(int cx = x0; cx <= x1 && !vis.empty(); ++cx)
            {
                for(const int pi : _cells[cy * G + cx])
                {
                    if(stamp[pi] == si)
                    {
                        continue;
                    }
                    stamp[pi] = si;

                    const Poly& p = _polys[pi];

                    // Hidden by later layers, and by its own layer's other
                    // shapes when it is a shape's edge.
                    if(p.layer < s.layer || pi == s.owner ||
                        (p.layer == s.layer && s.owner < 0))
                    {
                        continue;
                    }

                    if(p.layer == s.layer && pi < s.owner &&
                        (p.color.r != s.color.r || p.color.g != s.color.g ||
                            p.color.b != s.color.b))
                    {
                        continue; // drawn before it in another color: under it
                    }

                    if(p.maxx + SEAM < sminx || p.minx - SEAM > smaxx ||
                        p.maxy + SEAM < sminy || p.miny - SEAM > smaxy)
                    {
                        continue;
                    }

                    // Cyrus-Beck against the polygon grown by SEAM.
                    float tin = 0.f, tout = 1.f;
                    bool outside = false;
                    const V2* q = &_pts[p.first];

                    for(int k = 0; k < p.n && !outside; ++k)
                    {
                        const V2 e0 = q[k], e1 = q[(k + 1) % p.n];
                        const float ex = e1.x - e0.x, ey = e1.y - e0.y;
                        const float el = std::sqrt(ex * ex + ey * ey);
                        if(el < 1e-4f)
                        {
                            continue;
                        }

                        // Distance left of the edge, grown by SEAM.
                        const float f0 = cross(e0, e1, s.a) / el + SEAM;
                        const float fd = (ex * dy - ey * dx) / el;

                        if(std::fabs(fd) < 1e-6f)
                        {
                            if(f0 < 0.f) outside = true;
                            continue;
                        }

                        const float t = -f0 / fd;
                        if(fd > 0.f)
                        {
                            tin = std::max(tin, t);
                        }
                        else
                        {
                            tout = std::min(tout, t);
                        }

                        if(tin >= tout) outside = true;
                    }

                    if(!outside)
                    {
                        subtract(vis, tin, tout);
                    }
                }
            }
        }

        std::sort(vis.begin(), vis.end(),
            [](const Interval& a, const Interval& b) { return a.t0 < b.t0; });

        if(vis.empty())
        {
            continue;
        }

        for(const Interval& v : vis)
        {
            if((v.t1 - v.t0) * len < 2.f)
            {
                continue; // crumbs the beam would only make a dot of
            }

            if(s.owner >= 0) _visibleLen[s.owner] += (v.t1 - v.t0) * len;

            _pieces.push_back(Piece{{s.a.x + dx * v.t0, s.a.y + dy * v.t0},
                {s.a.x + dx * v.t1, s.a.y + dy * v.t1}, s.color, false});
        }
    }

    emit(d);
}

float Occluder::visibleFraction(int polygon) const
{
    if(polygon < 0 || polygon >= static_cast<int>(_outlineLen.size()) || _outlineLen[polygon] <= 0.f)
        return -1.f;
    return _visibleLen[polygon] / _outlineLen[polygon];
}

bool Occluder::polygonOnBoard(int polygon) const
{
    const Poly& p = _polys[polygon];
    return p.minx > 0.f && p.miny > 0.f && p.maxx < BOARD && p.maxy < BOARD;
}

// The visible pieces in the order the shapes were added, every piece in its
// own direction: the same order from frame to frame. (Sorting by the
// nearest next piece saves blank travel, but the order then changes from
// frame to frame as the shapes move, and the beam settles differently each
// time: the picture shakes. The user saw it on the tube, 2026-10-18.) A piece
// that starts where the last one ended joins on without a move.
void Occluder::emit(vp_dvg* d)
{
    _stats.pieces = static_cast<int>(_pieces.size());

    V2 at{-1e9f, -1e9f};
    Rgb color{0, 0, 0};
    bool colorSet = false;

    for(const Piece& p : _pieces)
    {
        if(!colorSet || p.color.r != color.r || p.color.g != color.g || p.color.b != color.b)
        {
            vp_color(d, p.color.r, p.color.g, p.color.b);
            color = p.color;
            colorSet = true;
        }

        const int ax = static_cast<int>(p.a.x + 0.5f), ay = static_cast<int>(p.a.y + 0.5f);
        if(std::abs(ax - static_cast<int>(at.x + 0.5f)) > 1 || std::abs(ay - static_cast<int>(at.y + 0.5f)) > 1)
        {
            vp_moveto(d, ax, ay);
        }

        vp_lineto(d, static_cast<int>(p.b.x + 0.5f), static_cast<int>(p.b.y + 0.5f));
        _stats.lit += std::hypot(p.b.x - p.a.x, p.b.y - p.a.y);
        at = p.b;
    }
}

} // namespace vp
