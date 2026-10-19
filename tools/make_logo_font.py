"""make_logo_font.py - the title's letters from the original's logo font.

Reads OpenSquare-Bold.ttf (the original's Assets, the font of its logo and
of the marquee), takes the letters of OPEN HEXAGON, flattens their quadratic
curves, cuts each letter's area into triangles (ear clipping, holes bridged
in) and writes src/vp/gen/logo_font.inc: per letter its advance, its
contours (for the lines) and its triangles (for the hidden lines and the
3D sides). Units: the font's em, y up, scaled so a capital is 1 high.

    python tools/make_logo_font.py [font.ttf]
"""
import os
import struct
import sys

LETTERS = "OPENHXAG"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    ROOT, "reference", "SSVOpenHexagon", "_RELEASE", "Assets", "OpenSquare-Bold.ttf")
OUT = os.path.join(ROOT, "src", "vp", "gen", "logo_font.inc")


# --- the TrueType tables we need ----------------------------------------------

class Font:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        n = struct.unpack(">H", self.d[4:6])[0]
        self.t = {}
        for i in range(n):
            tag, _, off, ln = struct.unpack(">4sIII", self.d[12 + 16 * i:28 + 16 * i])
            self.t[tag.decode()] = (off, ln)
        head = self.t["head"][0]
        self.upem = struct.unpack(">H", self.d[head + 18:head + 20])[0]
        self.locfmt = struct.unpack(">h", self.d[head + 50:head + 52])[0]
        maxp = self.t["maxp"][0]
        self.nglyphs = struct.unpack(">H", self.d[maxp + 4:maxp + 6])[0]
        hhea = self.t["hhea"][0]
        self.nhm = struct.unpack(">H", self.d[hhea + 34:hhea + 36])[0]
        self.cmap = self._cmap()

    def u16(self, o): return struct.unpack(">H", self.d[o:o + 2])[0]
    def i16(self, o): return struct.unpack(">h", self.d[o:o + 2])[0]

    def _cmap(self):
        base = self.t["cmap"][0]
        n = self.u16(base + 2)
        for i in range(n):
            pid, eid, off = struct.unpack(">HHI", self.d[base + 4 + 8 * i:base + 12 + 8 * i])
            sub = base + off
            if self.u16(sub) == 4 and pid in (0, 3):
                segx2 = self.u16(sub + 6)
                ends = sub + 14
                starts = ends + segx2 + 2
                deltas = starts + segx2
                ranges = deltas + segx2
                m = {}
                for s in range(segx2 // 2):
                    e = self.u16(ends + 2 * s); st = self.u16(starts + 2 * s)
                    dl = self.i16(deltas + 2 * s); ro = self.u16(ranges + 2 * s)
                    for c in range(st, e + 1):
                        if c == 0xFFFF: continue
                        if ro == 0: g = (c + dl) & 0xFFFF
                        else:
                            a = ranges + 2 * s + ro + 2 * (c - st)
                            g = self.u16(a)
                            if g: g = (g + dl) & 0xFFFF
                        m[c] = g
                return m
        raise SystemExit("no format 4 cmap")

    def advance(self, g):
        h = self.t["hmtx"][0]
        g = min(g, self.nhm - 1)
        return self.u16(h + 4 * g)

    def glyph(self, g):
        loca = self.t["loca"][0]
        if self.locfmt == 0:
            a = self.u16(loca + 2 * g) * 2; b = self.u16(loca + 2 * g + 2) * 2
        else:
            a, b = struct.unpack(">II", self.d[loca + 4 * g:loca + 4 * g + 8])
        if a == b: return []
        o = self.t["glyf"][0] + a
        nc = self.i16(o)
        if nc < 0: raise SystemExit("composite glyph")
        ends = [self.u16(o + 10 + 2 * i) for i in range(nc)]
        p = o + 10 + 2 * nc
        il = self.u16(p); p += 2 + il
        n = ends[-1] + 1
        flags = []
        while len(flags) < n:
            f = self.d[p]; p += 1
            flags.append(f)
            if f & 8:
                r = self.d[p]; p += 1
                flags += [f] * r
        xs = []; v = 0
        for f in flags:
            if f & 2:
                dx = self.d[p]; p += 1
                v += dx if f & 16 else -dx
            elif not f & 16:
                v += self.i16(p); p += 2
            xs.append(v)
        ys = []; v = 0
        for f in flags:
            if f & 4:
                dy = self.d[p]; p += 1
                v += dy if f & 32 else -dy
            elif not f & 32:
                v += self.i16(p); p += 2
            ys.append(v)
        contours = []; s = 0
        for e in ends:
            pts = [(xs[i], ys[i], bool(flags[i] & 1)) for i in range(s, e + 1)]
            contours.append(flatten(pts))
            s = e + 1
        return contours


def flatten(pts, steps=6):
    """Quadratic TrueType contour -> polygon."""
    n = len(pts)
    # Start on an on-curve point.
    start = next((i for i in range(n) if pts[i][2]), None)
    if start is None:
        a, b = pts[0], pts[1]
        pts = [((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, True)] + pts
        start = 0; n = len(pts)
    seq = pts[start:] + pts[:start]
    out = []
    cur = seq[0]
    out.append((cur[0], cur[1]))
    i = 1
    while i <= n:
        p = seq[i % n]
        if p[2]:
            out.append((p[0], p[1])); cur = p; i += 1
            continue
        nxt = seq[(i + 1) % n]
        end = nxt if nxt[2] else ((p[0] + nxt[0]) / 2, (p[1] + nxt[1]) / 2, True)
        for k in range(1, steps + 1):
            t = k / steps
            x = (1 - t) ** 2 * cur[0] + 2 * (1 - t) * t * p[0] + t * t * end[0]
            y = (1 - t) ** 2 * cur[1] + 2 * (1 - t) * t * p[1] + t * t * end[1]
            out.append((x, y))
        cur = end
        i += 2 if nxt[2] else 1
    # drop the repeated start and points on a line
    if out and abs(out[-1][0] - out[0][0]) < 1e-6 and abs(out[-1][1] - out[0][1]) < 1e-6:
        out.pop()
    clean = []
    for j, q in enumerate(out):
        a = out[j - 1]; b = out[(j + 1) % len(out)]
        cr = (q[0] - a[0]) * (b[1] - q[1]) - (q[1] - a[1]) * (b[0] - q[0])
        if abs(cr) > 1e-3: clean.append(q)
    return clean


# --- triangles: ear clipping with the holes bridged in ---------------------------

def area(poly):
    return sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
               for i in range(len(poly))) / 2


def inside(p, poly):
    c = False
    for i in range(len(poly)):
        a, b = poly[i], poly[i - 1]
        if (a[1] > p[1]) != (b[1] > p[1]) and p[0] < (b[0] - a[0]) * (p[1] - a[1]) / (b[1] - a[1]) + a[0]:
            c = not c
    return c


def seg_cross(a, b, c, d):
    def o(p, q, r): return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    d1, d2, d3, d4 = o(c, d, a), o(c, d, b), o(a, b, c), o(a, b, d)
    return (d1 * d2 < 0) and (d3 * d4 < 0)


def bridge(outer, hole):
    """Join a hole into the outer ring by a seam from the hole's rightmost point."""
    hi = max(range(len(hole)), key=lambda i: hole[i][0])
    hp = hole[hi]
    best = None
    for oi, op in enumerate(outer):
        if op[0] < hp[0] - 1e-9: continue
        ok = True
        for ring in (outer, hole):
            for k in range(len(ring)):
                a, b = ring[k], ring[(k + 1) % len(ring)]
                if a in (op, hp) or b in (op, hp): continue
                if seg_cross(hp, op, a, b): ok = False; break
            if not ok: break
        if ok:
            dd = (op[0] - hp[0]) ** 2 + (op[1] - hp[1]) ** 2
            if best is None or dd < best[0]: best = (dd, oi)
    if best is None:
        best = (0, min(range(len(outer)), key=lambda i: (outer[i][0] - hp[0]) ** 2 + (outer[i][1] - hp[1]) ** 2))
    oi = best[1]
    h = hole[hi:] + hole[:hi]
    return outer[:oi + 1] + h + [h[0], outer[oi]] + outer[oi + 1:]


def earclip(poly):
    pts = list(poly)
    tris = []
    guard = 0
    while len(pts) > 3 and guard < 10000:
        guard += 1
        n = len(pts)
        cut = False
        for i in range(n):
            a, b, c = pts[i - 1], pts[i], pts[(i + 1) % n]
            cr = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
            if cr <= 1e-9: continue
            ear = True
            for q in pts:
                if q in (a, b, c): continue
                d1 = (b[0] - a[0]) * (q[1] - a[1]) - (b[1] - a[1]) * (q[0] - a[0])
                d2 = (c[0] - b[0]) * (q[1] - b[1]) - (c[1] - b[1]) * (q[0] - b[0])
                d3 = (a[0] - c[0]) * (q[1] - c[1]) - (a[1] - c[1]) * (q[0] - c[0])
                if d1 >= 0 and d2 >= 0 and d3 >= 0: ear = False; break
            if ear:
                tris.append((a, b, c)); pts.pop(i); cut = True
                break
        if not cut:
            pts.pop(0)  # degenerate: give up a point
    if len(pts) == 3: tris.append(tuple(pts))
    return tris


def triangulate(contours):
    rings = [list(c) for c in contours if len(c) >= 3]
    # Outer rings are those inside no other ring (the font's winding is not
    # relied on); outers anticlockwise, holes clockwise.
    def contains(a, b):
        return all(inside(p, a) for p in b[:3])
    outers, holes = [], []
    for r in rings:
        depth = sum(1 for o in rings if o is not r and abs(area(o)) > abs(area(r)) and contains(o, r))
        (holes if depth % 2 else outers).append(r)
    outers = [r if area(r) > 0 else r[::-1] for r in outers]
    holes = [r if area(r) < 0 else r[::-1] for r in holes]
    tris = []
    for o in outers:
        mine = [h for h in holes if contains(o, h)]
        mine.sort(key=lambda h: -max(p[0] for p in h))
        poly = o
        for h in mine:
            poly = bridge(poly, h)
        tris += earclip(poly)
    return tris


def main():
    f = Font(FONT)
    cap = None
    data = {}
    for ch in LETTERS:
        g = f.cmap[ord(ch)]
        cs = f.glyph(g)
        data[ch] = (f.advance(g), cs)
        top = max(p[1] for c in cs for p in c)
        cap = top if cap is None else max(cap, top)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", newline="\n") as o:
        o.write("// Made by tools/make_logo_font.py from OpenSquare-Bold.ttf (the original's\n")
        o.write("// logo font, Assets/). Do not edit. A capital is 1 high, y up.\n")
        for ch in LETTERS:
            adv, cs = data[ch]
            tris = triangulate(cs)
            s = 1.0 / cap
            o.write("static const float LOGO_%s_CONTOURS[] = {\n" % ch)
            for c in cs:
                o.write("    %d, " % len(c) + ", ".join("%.4ff, %.4ff" % (x * s, y * s) for x, y in c) + ",\n")
            o.write("    0};\n")
            o.write("static const float LOGO_%s_TRIS[] = {\n" % ch)
            for t in tris:
                o.write("    " + ", ".join("%.4ff, %.4ff" % (x * s, y * s) for x, y in t) + ",\n")
            o.write("};\n")
            o.write("static const LogoGlyph LOGO_%s = {'%s', %.4ff, LOGO_%s_CONTOURS, LOGO_%s_TRIS, %d};\n\n"
                    % (ch, ch, adv * s, ch, ch, len(tris)))
            print(ch, "contours", len(cs), "points", sum(len(c) for c in cs), "triangles", len(tris))
    print("wrote", OUT)


main()
