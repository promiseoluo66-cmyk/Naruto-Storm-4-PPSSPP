#!/usr/bin/env python3
"""
Convert a Wavefront OBJ into this project's NMDL v2 mesh format.

Usage:  python3 tools/obj2nmdl.py input.obj output.nmdl [--no-flip-v]

Supports: v x y z [r g b]   (vertex colours 0..1, as exported by Blender/MeshLab)
          vt u v
          f a/b/c ...        (v, v/vt, v/vt/vn, v//vn; quads and n-gons are
                              triangulated as fans)
Ignores normals, materials and groups.

NMDL v2 (little-endian):  "NMDL", u32 version=2, u32 vertex_count, then per
vertex 24 bytes: f32 u, f32 v, u32 colour (R in the low byte), f32 x, y, z.
Limits enforced to match src/model.c: vertex_count multiple of 3, <= 120000.
"""
import struct
import sys

MAX_VERTICES = 120000


def convert(src, dst, flip_v=True):
    pos, col, uv = [], [], []
    tris = []
    with open(src, "r") as f:
        for raw in f:
            parts = raw.split("#", 1)[0].split()
            if not parts:
                continue
            tag = parts[0]
            if tag == "v":
                x, y, z = (float(parts[1]), float(parts[2]), float(parts[3]))
                if len(parts) >= 7:
                    c = tuple(max(0.0, min(1.0, float(p))) for p in parts[4:7])
                else:
                    c = (1.0, 1.0, 1.0)
                pos.append((x, y, z))
                col.append(c)
            elif tag == "vt":
                uv.append((float(parts[1]), float(parts[2]) if len(parts) > 2 else 0.0))
            elif tag == "f":
                refs = []
                for tok in parts[1:]:
                    bits = tok.split("/")
                    vi = int(bits[0])
                    vi = vi - 1 if vi > 0 else len(pos) + vi
                    ti = None
                    if len(bits) > 1 and bits[1]:
                        t = int(bits[1])
                        ti = t - 1 if t > 0 else len(uv) + t
                    refs.append((vi, ti))
                for i in range(1, len(refs) - 1):
                    tris.append((refs[0], refs[i], refs[i + 1]))

    if not tris:
        raise SystemExit("no faces found in " + src)
    count = len(tris) * 3
    if count > MAX_VERTICES:
        raise SystemExit("too many vertices: %d (max %d)" % (count, MAX_VERTICES))

    out = bytearray(b"NMDL")
    out += struct.pack("<II", 2, count)
    for tri in tris:
        for vi, ti in tri:
            x, y, z = pos[vi]
            r, g, b = col[vi]
            colour = (255 << 24) | (int(b * 255 + 0.5) << 16) | (int(g * 255 + 0.5) << 8) | int(r * 255 + 0.5)
            u, v = (0.0, 0.0)
            if ti is not None and 0 <= ti < len(uv):
                u, v = uv[ti]
                if flip_v:
                    v = 1.0 - v
            out += struct.pack("<ffIfff", u, v, colour, x, y, z)
    with open(dst, "wb") as f:
        f.write(out)
    print("wrote %s: %d triangles, %d vertices, %d bytes" % (dst, len(tris), count, len(out)))


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        raise SystemExit(__doc__)
    convert(args[0], args[1], flip_v="--no-flip-v" not in sys.argv)
