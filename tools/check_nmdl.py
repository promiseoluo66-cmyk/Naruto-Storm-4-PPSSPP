#!/usr/bin/env python3
"""Validate NMDL v2 / NMTX v1 files the same way src/model.c does, and optionally
render a software preview PNG (needs Pillow; this is NOT PSP output, just a sanity check).

Usage: python3 tools/check_nmdl.py model.nmdl [texture.nmtx] [preview.png]
"""
import struct, sys, math


def load_nmdl(path):
    d = open(path, "rb").read()
    magic, ver, n = struct.unpack_from("<4sII", d, 0)
    assert magic == b"NMDL", "bad magic"
    assert ver == 2, "version %d" % ver
    assert 0 < n <= 120000 and n % 3 == 0, "bad vertex count %d" % n
    assert len(d) == 12 + n * 24, "size mismatch: %d vs %d" % (len(d), 12 + n * 24)
    return [struct.unpack_from("<ffIfff", d, 12 + i * 24) for i in range(n)]


def load_nmtx(path):
    d = open(path, "rb").read()
    magic, ver, w, h, fmt = struct.unpack_from("<4sIIII", d, 0)
    assert magic == b"NMTX" and ver == 1
    assert fmt in (1, 3)
    for v in (w, h):
        assert 8 <= v <= 512 and (v & (v - 1)) == 0, "dimension %d" % v
    bpp = 2 if fmt == 1 else 4
    assert len(d) == 20 + w * h * bpp, "texture size mismatch"
    return w, h, fmt


def preview(verts, out, size=360):
    from PIL import Image, ImageDraw
    img = Image.new("RGB", (size * 2, size), (40, 50, 80))
    dr = ImageDraw.Draw(img)
    for view, ox in ((0.35, 0), (math.pi + 0.35, size)):
        c, s = math.cos(view), math.sin(view)
        tris = []
        for i in range(0, len(verts), 3):
            pts, cols = [], []
            for (_, _, colr, x, y, z) in verts[i:i + 3]:
                pts.append((x * c + z * s, y, -x * s + z * c))
                cols.append((colr & 255, (colr >> 8) & 255, (colr >> 16) & 255))
            depth = sum(p[2] for p in pts) / 3
            a = [pts[1][k] - pts[0][k] for k in range(3)]
            b = [pts[2][k] - pts[0][k] for k in range(3)]
            nx, ny, nz = a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]
            ln = math.sqrt(nx * nx + ny * ny + nz * nz) or 1
            light = 0.55 + 0.45 * max(0.0, (nx * 0.3 + ny * 0.6 + nz * 0.7) / ln)
            col = tuple(int(sum(c_[k] for c_ in cols) / 3 * light) for k in range(3))
            tris.append((depth, pts, col))
        tris.sort(key=lambda t: t[0], reverse=True)
        for depth, pts, col in tris:
            poly = [(ox + size / 2 + p[0] * size * 0.36, size * 0.93 - p[1] * size * 0.38) for p in pts]
            dr.polygon(poly, fill=col)
    img.save(out)


if __name__ == "__main__":
    v = load_nmdl(sys.argv[1])
    print("NMDL OK: %d vertices, %d triangles" % (len(v), len(v) // 3))
    if len(sys.argv) > 2 and sys.argv[2].endswith(".nmtx"):
        print("NMTX OK: %dx%d fmt %d" % load_nmtx(sys.argv[2]))
    if len(sys.argv) > 3:
        preview(v, sys.argv[3])
        print("preview written", sys.argv[3])
