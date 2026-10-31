#!/usr/bin/env python3
"""Convert the rigged Naruto SMD in the supplied archive to the game's NSKM format.

NSKM v1 stores the SMD reference-pose skeleton, weighted triangle vertices and
an atlas texture. Animation clips are not invented: this archive's SMD files
contain a single time-0 skeleton sample, so movement/combat poses are generated
procedurally by the PSP runtime.
"""
import io
import math
import os
import posixpath
import struct
import sys
import zipfile
from collections import Counter
from PIL import Image

ATLAS_SIZE = 512
TILE_SIZE = 256
MAX_BONES = 256
MAX_VERTICES = 60000
MAX_INFLUENCES = 4

def mat_from_smd(tx, ty, tz, rx, ry, rz):
    """SMD Euler angles (radians), composed as Rz * Ry * Rx, plus translation."""
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)
    # Row-major 3x4 affine matrix.
    return (
        cz*cy, cz*sy*sx - sz*cx, cz*sy*cx + sz*sx, tx,
        sz*cy, sz*sy*sx + cz*cx, sz*sy*cx - cz*sx, ty,
        -sy,   cy*sx,             cy*cx,              tz,
    )

def parse_smd(text):
    lines = text.splitlines()
    nodes = {}
    skeleton_frames = {}
    triangles = []
    section = None
    i = 0

    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if not line:
            continue
        if line in ("nodes", "skeleton", "triangles"):
            section = line
            continue
        if line == "end":
            section = None
            continue

        if section == "nodes":
            parts = line.split(None, 2)
            if len(parts) != 3:
                raise ValueError("Malformed SMD node line: " + line)
            bone_id = int(parts[0])
            name = parts[1].strip('"')
            parent = int(parts[2])
            nodes[bone_id] = (name, parent)

        elif section == "skeleton":
            if line.startswith("time "):
                time_id = int(line.split()[1])
                frame = {}
                while i < len(lines):
                    sample = lines[i].strip()
                    if not sample:
                        i += 1
                        continue
                    if sample.startswith("time ") or sample == "end":
                        break
                    p = sample.split()
                    if len(p) != 7:
                        raise ValueError("Malformed SMD bone transform: " + sample)
                    frame[int(p[0])] = tuple(map(float, p[1:7]))
                    i += 1
                skeleton_frames[time_id] = frame

        elif section == "triangles":
            material = line
            verts = []
            for _ in range(3):
                if i >= len(lines):
                    raise ValueError("Unexpected end of SMD triangle list")
                row = lines[i].strip()
                i += 1
                while not row and i < len(lines):
                    row = lines[i].strip()
                    i += 1
                if not row or row == "end":
                    raise ValueError("Incomplete SMD triangle")
                p = row.split()
                if len(p) < 9:
                    raise ValueError("SMD vertex has fewer than 9 fields: " + row)
                parent_bone = int(p[0])
                pos = tuple(map(float, p[1:4]))
                normal = tuple(map(float, p[4:7]))
                uv = tuple(map(float, p[7:9]))
                influences = []
                if len(p) > 9:
                    count = int(p[9])
                    if count < 0 or len(p) < 10 + count * 2:
                        raise ValueError("Invalid SMD influence list: " + row)
                    for j in range(count):
                        influences.append((int(p[10 + j*2]), float(p[11 + j*2])))
                if not influences:
                    influences = [(parent_bone, 1.0)]
                influences.sort(key=lambda x: x[1], reverse=True)
                influences = influences[:MAX_INFLUENCES]
                total = sum(max(0.0, w) for _, w in influences)
                if total <= 1e-8:
                    influences = [(parent_bone, 1.0)]
                    total = 1.0
                influences = [(b, max(0.0, w) / total) for b, w in influences]
                verts.append((pos, normal, uv, influences))
            triangles.append((material, verts))

    if not nodes:
        raise ValueError("SMD has no nodes section")
    if not skeleton_frames:
        raise ValueError("SMD has no skeleton reference pose")
    if 0 not in skeleton_frames:
        raise ValueError("SMD skeleton is missing time 0 reference pose")
    ids = sorted(nodes)
    if ids != list(range(len(ids))):
        raise ValueError("Bone IDs must be dense from 0; got " + str(ids[:10]))
    if len(ids) > MAX_BONES:
        raise ValueError("Skeleton exceeds runtime bone limit")
    for bone_id, (_, parent) in nodes.items():
        if parent >= bone_id or parent < -1:
            raise ValueError("Bone hierarchy must be parent-before-child; invalid parent for " + str(bone_id))
    if not triangles:
        raise ValueError("SMD has no triangle mesh")
    return nodes, skeleton_frames, triangles

def resolve_material_textures(names, mtl_text, materials):
    by_lower = {n.replace("\\", "/").lower(): n for n in names}
    by_base = {}
    for n in names:
        by_base.setdefault(posixpath.basename(n.replace("\\", "/")).lower(), n)

    mtl = {}
    current = None
    for raw in mtl_text.splitlines():
        p = raw.strip().split()
        if not p:
            continue
        if p[0] == "newmtl" and len(p) > 1:
            current = p[1]
        elif p[0] == "map_Kd" and current:
            ref = p[-1].replace("\\", "/")
            mtl[current] = ref

    resolved = {}
    for material in materials:
        candidates = []
        if material in mtl:
            candidates.append(mtl[material])
        candidates.append(material)
        for ref in candidates:
            key = ref.replace("\\", "/").lower()
            full = by_lower.get(key)
            if not full:
                full = by_base.get(posixpath.basename(key))
            if full:
                resolved[material] = full
                break
        if material not in resolved:
            # Texture filenames in this archive are sometimes the material names.
            base = posixpath.basename(material.replace("\\", "/")).lower()
            hit = by_base.get(base)
            if hit:
                resolved[material] = hit
    missing = [m for m in materials if m not in resolved]
    if missing:
        raise ValueError("No texture found for SMD materials: " + ", ".join(missing))
    paths = []
    for material in materials:
        if resolved[material] not in paths:
            paths.append(resolved[material])
    if len(paths) > 4:
        raise ValueError("SMD needs more than four texture atlas tiles")
    return resolved, paths

def make_atlas(inner, texture_paths):
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE), (0, 0, 0, 0))
    regions = {}
    resample = getattr(Image, "Resampling", Image).LANCZOS
    for slot, path in enumerate(texture_paths):
        with Image.open(io.BytesIO(inner.read(path))) as source:
            im = source.convert("RGBA")
        im.thumbnail((TILE_SIZE - 4, TILE_SIZE - 4), resample)
        tile_x = (slot % 2) * TILE_SIZE
        tile_y = (slot // 2) * TILE_SIZE
        x = tile_x + (TILE_SIZE - im.width) // 2
        y = tile_y + (TILE_SIZE - im.height) // 2
        atlas.paste(im, (x, y))
        regions[path] = (x, y, im.width, im.height)
        print("ATLAS TILE", slot, path, im.width, im.height)
    return atlas, regions

def convert(archive_path, output_prefix):
    with zipfile.ZipFile(archive_path) as outer:
        nested = next((n for n in outer.namelist() if n.lower().endswith(".zip")), None)
        if not nested:
            raise ValueError("Outer archive does not contain the model ZIP")
        with zipfile.ZipFile(io.BytesIO(outer.read(nested))) as inner:
            names = inner.namelist()
            smd_path = next((n for n in names if n.lower().endswith("/naruto.smd")), None)
            mtl_path = next((n for n in names if n.lower().endswith("/naruto.mtl")), None)
            if not smd_path or not mtl_path:
                raise ValueError("Naruto.smd or Naruto.mtl missing from nested archive")

            nodes, frames, triangles = parse_smd(inner.read(smd_path).decode("utf-8", "replace"))
            frame0 = frames[0]
            if set(frame0) != set(nodes):
                missing = sorted(set(nodes) - set(frame0))
                raise ValueError("Reference pose does not define every bone: " + str(missing[:12]))

            materials = []
            for material, _ in triangles:
                if material not in materials:
                    materials.append(material)
            resolved, paths = resolve_material_textures(
                names, inner.read(mtl_path).decode("utf-8", "replace"), materials)
            atlas, regions = make_atlas(inner, paths)

            vertex_count = len(triangles) * 3
            if vertex_count > MAX_VERTICES:
                raise ValueError("Mesh exceeds runtime vertex limit")
            bones_blob = bytearray()
            for bone_id in range(len(nodes)):
                name, parent = nodes[bone_id]
                tx, ty, tz, rx, ry, rz = frame0[bone_id]
                local = mat_from_smd(tx, ty, tz, rx, ry, rz)
                encoded_name = name.encode("utf-8")[:31].ljust(32, b"\0")
                bones_blob += struct.pack("<i32s12f", parent, encoded_name, *local)

            verts_blob = bytearray()
            influence_hist = Counter()
            for material, verts in triangles:
                tex_path = resolved[material]
                x0, y0, tw, th = regions[tex_path]
                for pos, normal, uv, influences in verts:
                    u, v = uv
                    atlas_u = (x0 + 0.5 + u * max(0, tw - 1)) / ATLAS_SIZE
                    atlas_v = (y0 + 0.5 + (1.0 - v) * max(0, th - 1)) / ATLAS_SIZE
                    bone_ids = [0] * 4
                    weights = [0.0] * 4
                    for slot, (bone_id, weight) in enumerate(influences):
                        if bone_id < 0 or bone_id >= len(nodes):
                            raise ValueError("Vertex references invalid bone " + str(bone_id))
                        bone_ids[slot] = bone_id
                        weights[slot] = weight
                    influence_hist[len(influences)] += 1
                    verts_blob += struct.pack(
                        "<8f4H4f",
                        *pos, *normal, atlas_u, atlas_v, *bone_ids, *weights
                    )

            os.makedirs(os.path.dirname(output_prefix) or ".", exist_ok=True)
            with open(output_prefix + ".nskl", "wb") as f:
                f.write(struct.pack("<4sIIII", b"NSKM", 1, len(nodes), vertex_count, len(frames)))
                f.write(bones_blob)
                f.write(verts_blob)
            with open(output_prefix + ".nmtx", "wb") as f:
                f.write(struct.pack("<4sIIII", b"NMTX", 1, ATLAS_SIZE, ATLAS_SIZE, 3))
                f.write(atlas.tobytes())

            print("SKELETAL IMPORT OK")
            print("SOURCE", smd_path)
            print("BONES", len(nodes), "VERTICES", vertex_count, "TRIANGLES", len(triangles))
            print("REFERENCE POSE FRAMES", len(frames), "(time 0 only is used as bind pose)")
            print("WEIGHT INFLUENCE HISTOGRAM", dict(influence_hist))
            print("ANIMATION CLIPS", 0, "(source archive contains reference poses, not motion clips)")
            print("OUTPUT", output_prefix + ".nskl", os.path.getsize(output_prefix + ".nskl"))
            print("OUTPUT", output_prefix + ".nmtx", os.path.getsize(output_prefix + ".nmtx"))

if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: python3 tools/import_smd_zip.py source.zip output_prefix")
    try:
        convert(sys.argv[1], sys.argv[2])
    except Exception as exc:
        raise SystemExit("SMD import failed: " + str(exc))
