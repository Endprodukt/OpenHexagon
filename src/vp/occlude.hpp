// Hidden lines in the plane, for the vector board.
//
// The original paints filled shapes over each other: later shapes cover
// earlier ones, and shapes of one group in one color (the walls, the 3D
// band under them, the pivot) merge into one region. On the beam only
// outlines exist, so this does the painting for lines:
//
//   * every shape belongs to a layer (draw order) and adds its outline;
//   * a piece of an outline is hidden where a shape of a later layer covers
//     it, where a shape of its own layer and color does (one region: the
//     edge two walls share is not drawn), or where a shape of its own layer
//     drawn after it does (paint over paint);
//   * what is left is drawn in the order it was added, the same every
//     frame (no sorting: a changing order makes the picture shake on the
//     tube), pieces that meet joined into one stroke.
//
// Shapes are convex polygons (the original's quads and triangles, and the
// convex hull of a shape and its 3D offset). Coordinates are board units.
#pragma once

#include <cstdint>
#include <vector>

typedef struct vp_dvg vp_dvg;

namespace vp {

struct V2
{
    float x, y;
};

struct Rgb
{
    std::uint8_t r, g, b;
};

class Occluder
{
public:
    void clear();

    // A filled convex polygon of `n` corners (either winding) on `layer`.
    // `outline`: draw its edges in `color`; else it only hides.
    void addPolygon(const V2* pts, int n, int layer, Rgb color, bool outline);

    // A lone line on `layer` (the background's spokes): hidden by the shapes
    // of later layers only.
    void addLine(V2 a, V2 b, int layer, Rgb color);

    // Lines are drawn only within this distance of the board's middle
    // (0: the whole board). Set before draw().
    void setClipRadius(float r) { _clipRadius = r; }

    // Cut, then draw. Lines outside the board are clipped to it.
    void draw(vp_dvg* d);

    struct Stats
    {
        int polygons, segments, pieces;
        float lit; // board units the beam draws lit
    };

    [[nodiscard]] Stats stats() const { return _stats; }

    // For the self test, after draw(): the part of a polygon's outline
    // (by addPolygon's order, from 0) left visible, 0..1; -1 if it has none.
    [[nodiscard]] float visibleFraction(int polygon) const;
    [[nodiscard]] int polygonCount() const { return static_cast<int>(_polys.size()); }
    [[nodiscard]] int polygonLayer(int polygon) const { return _polys[polygon].layer; }
    [[nodiscard]] bool polygonOnBoard(int polygon) const;

private:
    struct Poly
    {
        int first, n;          // corners in _pts, anticlockwise
        int layer;
        Rgb color;
        float minx, miny, maxx, maxy;
    };

    struct Seg
    {
        V2 a, b;
        int layer;
        int owner;             // its polygon, or -1
        Rgb color;
    };

    struct Piece
    {
        V2 a, b;
        Rgb color;
        bool used;
    };

    std::vector<Piece> _pieces;
    std::vector<float> _outlineLen, _visibleLen; // per polygon
    std::vector<V2> _pts;
    std::vector<Poly> _polys;
    std::vector<Seg> _segs;
    Stats _stats{};
    float _clipRadius{0.f};

    // Coarse screen grid of polygon indices, built in draw().
    static constexpr int G = 16;
    std::vector<int> _cells[G * G];

    void buildGrid();
    void emit(vp_dvg* d);
};

} // namespace vp
