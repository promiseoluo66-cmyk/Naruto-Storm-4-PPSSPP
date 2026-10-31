#!/usr/bin/env python3
"""Read-only Naruto SMD rig audit; no gameplay or importer files are modified."""
import argparse
import csv
import hashlib
import io
import json
import math
import re
import shlex
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ARCHIVE = ROOT / "assets/chr/test/naruto-shippuden-naruto.zip"


def mat_from_smd(tx, ty, tz, rx, ry, rz):
    # Same convention as tools/import_smd_zip.py: row-major Rz * Ry * Rx + T.
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)
    return [
        cz*cy, cz*sy*sx-sz*cx, cz*sy*cx+sz*sx, tx,
        sz*cy, sz*sy*sx+cz*cx, sz*sy*cx-cz*sx, ty,
        -sy, cy*sx, cy*cx, tz,
    ]


def mul(a, b):
    out = [0.0] * 12
    for r in range(3):
        for c in range(3):
            out[r*4+c] = sum(a[r*4+k] * b[k*4+c] for k in range(3))
        out[r*4+3] = a[r*4]*b[3] + a[r*4+1]*b[7] + a[r*4+2]*b[11] + a[r*4+3]
    return out


def inv_rigid(m):
    # Mirrors src/skinned.c mat_inverse_rigid().
    o = [0.0] * 12
    o[0], o[1], o[2] = m[0], m[4], m[8]
    o[4], o[5], o[6] = m[1], m[5], m[9]
    o[8], o[9], o[10] = m[2], m[6], m[10]
    o[3] = -(o[0]*m[3] + o[1]*m[7] + o[2]*m[11])
    o[7] = -(o[4]*m[3] + o[5]*m[7] + o[6]*m[11])
    o[11] = -(o[8]*m[3] + o[9]*m[7] + o[10]*m[11])
    return o


def parse_nodes_frames(text):
    lines = text.splitlines()
    nodes, frames = {}, {}
    section, i = None, 0
    while i < len(lines):
        raw = lines[i].strip()
        i += 1
        if not raw:
            continue
        if raw in ("nodes", "skeleton", "triangles"):
            section = raw
            continue
        if raw == "end":
            section = None
            continue
        if section == "nodes":
            p = shlex.split(raw)
            if len(p) != 3:
                raise ValueError("Malformed node declaration: " + raw)
            bid, name, parent = int(p[0]), p[1], int(p[2])
            if bid in nodes:
                raise ValueError("Duplicate bone ID " + str(bid))
            nodes[bid] = {"id": bid, "name": name, "parent": parent}
        elif section == "skeleton" and raw.startswith("time "):
            tid = int(raw.split()[1])
            if tid in frames:
                raise ValueError("Duplicate frame time " + str(tid))
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
                    raise ValueError("Malformed transform: " + sample)
                bid = int(p[0])
                if bid in frame:
                    raise ValueError("Duplicate bone transform " + str(bid))
                frame[bid] = [float(v) for v in p[1:]]
                i += 1
            frames[tid] = frame
    if not nodes or 0 not in frames:
        raise ValueError("Missing nodes or time-0 reference pose")
    return nodes, frames


def parse_vertices(text, bone_count):
    lines = text.splitlines()
    start = next((i for i, x in enumerate(lines) if x.strip() == "triangles"), None)
    if start is None:
        raise ValueError("Missing triangles section")
    i, records, materials = start + 1, [], []
    while i < len(lines) and lines[i].strip() != "end":
        material = lines[i].strip()
        i += 1
        if not material:
            continue
        materials.append(material)
        for corner in range(3):
            while i < len(lines) and not lines[i].strip():
                i += 1
            if i >= len(lines) or lines[i].strip() == "end":
                raise ValueError("Incomplete triangle record")
            p = lines[i].split()
            i += 1
            if len(p) < 9:
                raise ValueError("Malformed vertex record")
            parent = int(p[0])
            pos = [float(x) for x in p[1:4]]
            normal = [float(x) for x in p[4:7]]
            uv = [float(x) for x in p[7:9]]
            if len(p) > 9:
                count = int(p[9])
                if count < 0 or len(p) != 10 + 2*count:
                    raise ValueError("Malformed influence block")
                inf = [{"bone": int(p[10+2*k]), "weight": float(p[11+2*k])} for k in range(count)]
            else:
                inf = [{"bone": parent, "weight": 1.0}]
            if not inf:
                inf = [{"bone": parent, "weight": 1.0}]
            for x in inf:
                if x["bone"] < 0 or x["bone"] >= bone_count or not math.isfinite(x["weight"]) or x["weight"] < 0:
                    raise ValueError("Invalid vertex influence")
            if sum(x["weight"] for x in inf) <= 1e-8:
                raise ValueError("Zero total vertex weight")
            records.append({
                "record": len(records), "triangle": len(records)//3, "corner": corner,
                "material": material, "parent_bone": parent, "position": pos,
                "normal": normal, "uv": uv, "influences": inf,
            })
    return records, materials


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--archive", type=Path, default=DEFAULT_ARCHIVE)
    ap.add_argument("--out", type=Path, default=ROOT / "audit-output")
    args = ap.parse_args()
    outer_bytes = args.archive.read_bytes()
    with zipfile.ZipFile(io.BytesIO(outer_bytes)) as outer:
        nested = [n for n in outer.namelist() if n.lower().endswith(".zip")]
        if len(nested) != 1:
            raise ValueError("Expected exactly one nested ZIP; got " + str(len(nested)))
        with zipfile.ZipFile(io.BytesIO(outer.read(nested[0]))) as inner:
            smds = [n for n in inner.namelist() if n.lower().endswith(".smd")]
            selected = next((n for n in smds if n.endswith("/Naruto.smd")), None)
            if selected is None:
                raise ValueError("Importer-selected Naruto.smd not found")
            smd_bytes = inner.read(selected)
            smd_text = smd_bytes.decode("utf-8")
            nodes, frames = parse_nodes_frames(smd_text)
            vertices, materials = parse_vertices(smd_text, len(nodes))
            alternate = None
            alt_path = next((n for n in smds if n.endswith("/Naruto_9T.smd")), None)
            if alt_path:
                alt_nodes, alt_frames = parse_nodes_frames(inner.read(alt_path).decode("utf-8"))
                alt_vertices, _ = parse_vertices(inner.read(alt_path).decode("utf-8"), len(alt_nodes))
                alternate = {
                    "path": alt_path, "bones": len(alt_nodes),
                    "frame_times": sorted(alt_frames),
                    "triangle_corner_records": len(alt_vertices),
                    "same_bone_names_and_parents": len(alt_nodes) == len(nodes) and all(
                        alt_nodes[i]["name"] == nodes[i]["name"] and alt_nodes[i]["parent"] == nodes[i]["parent"]
                        for i in nodes
                    ),
                }

    ids = sorted(nodes)
    if ids != list(range(len(ids))):
        raise ValueError("Bone IDs are not a dense zero-based sequence")
    if set(frames[0]) != set(ids):
        raise ValueError("Time-0 frame does not define exactly every node")
    children, roots = defaultdict(list), []
    for bid in ids:
        parent = nodes[bid]["parent"]
        if parent == -1:
            roots.append(bid)
        elif parent not in nodes or parent >= bid:
            raise ValueError("Invalid or non-parent-before-child hierarchy at bone " + str(bid))
        else:
            children[parent].append(bid)
    # Check for cycles independently of parent ordering.
    for bid in ids:
        seen, cur = set(), bid
        while cur != -1:
            if cur in seen:
                raise ValueError("Hierarchy cycle at bone " + str(cur))
            seen.add(cur)
            cur = nodes[cur]["parent"]

    runtime = (ROOT / "src/skinned.c").read_text(encoding="utf-8")
    runtime_names = sorted(set(re.findall(r'name_is\s*\(\s*n\s*,\s*"([^"]+)"\s*\)', runtime)))
    source_names = defaultdict(list)
    for bid in ids:
        source_names[nodes[bid]["name"]].append(bid)
    runtime_matches = [{"name": n, "bone_ids": source_names[n]} for n in runtime_names if n in source_names]
    missing_runtime = [n for n in runtime_names if n not in source_names]

    out_bones, matrix_residuals = [], []
    encoded_names = defaultdict(list)
    for bid in ids:
        n = nodes[bid]
        tx, ty, tz, rx, ry, rz = frames[0][bid]
        local = mat_from_smd(tx, ty, tz, rx, ry, rz)
        parent = n["parent"]
        global_m = local if parent == -1 else mul(out_bones[parent]["global"], local)
        inverse = inv_rigid(global_m)
        test = mul(global_m, inverse)
        residual = max(abs(test[r*4+c] - (1.0 if r == c else 0.0)) for r in range(3) for c in range(4))
        matrix_residuals.append(residual)
        encoded_names[n["name"].encode("utf-8")[:31]].append(bid)
        out_bones.append({
            **n, "children": children[bid],
            "translation": [tx, ty, tz], "rotation_euler_radians": [rx, ry, rz],
            "local_bind_3x4_row_major": local,
            "global_bind_3x4_row_major": global_m,
            "inverse_bind_3x4_row_major": inverse,
            "global_times_inverse_bind_max_residual": residual,
        })

    hist = Counter(len(v["influences"]) for v in vertices)
    max_inf = max(hist) if hist else 0
    over4 = sum(v["record"] >= 0 and len(v["influences"]) > 4 for v in vertices)
    discarded_weight = sum(
        sum(x["weight"] for x in sorted(v["influences"], key=lambda x: x["weight"], reverse=True)[4:])
        for v in vertices
    )
    truncated_collisions = {
        k.decode("utf-8", "replace"): v for k, v in encoded_names.items() if len(v) > 1
    }
    overlong_names = [{"id": b["id"], "name": b["name"], "utf8_bytes": len(b["name"].encode("utf-8"))}
                      for b in out_bones if len(b["name"].encode("utf-8")) > 31]
    position_count = len({tuple(v["position"]) for v in vertices})
    full_attribute_count = len({
        (tuple(v["position"]), tuple(v["normal"]), tuple(v["uv"]),
         tuple((x["bone"], x["weight"]) for x in v["influences"])) for v in vertices
    })

    def hierarchy_branch(bid, depth, lines):
        lines.append("  " * depth + f'{bid}: {nodes[bid]["name"]} (parent={nodes[bid]["parent"]})')
        for child in children[bid]:
            hierarchy_branch(child, depth + 1, lines)
    hierarchy_lines = []
    for root in roots:
        hierarchy_branch(root, 0, hierarchy_lines)

    archive_sha = hashlib.sha256(outer_bytes).hexdigest()
    smd_sha = hashlib.sha256(smd_bytes).hexdigest()
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = {
        "source": {
            "outer_archive": str(args.archive.relative_to(ROOT)) if args.archive.is_relative_to(ROOT) else str(args.archive),
            "outer_archive_sha256": archive_sha,
            "nested_archive": nested[0], "smd_path": selected, "smd_sha256": smd_sha,
            "alternate_smd": alternate,
        },
        "matrix_convention": {
            "local": "SMD Euler angles in radians composed Rz * Ry * Rx; row-major 3x4 affine",
            "global": "global(child) = global(parent) * local(child)",
            "inverse_bind": "Rigid inverse by 3x3 transpose and inverse translation, matching src/skinned.c",
            "runtime_coordinate_mapping": "After skinning, src/skinned.c maps position (x,y,z) to (x,z,-y)",
            "coordinate_basis_note": "This reports the implementation's mapping; SMD has no embedded coordinate-system declaration to authenticate the exporter basis.",
        },
        "summary": {
            "bone_count": len(ids), "id_range": [min(ids), max(ids)], "root_ids": roots,
            "frame_times": sorted(frames), "reference_pose_bone_count": len(frames[0]),
            "triangle_count": len(vertices)//3, "triangle_corner_record_count": len(vertices),
            "distinct_position_tuples_diagnostic_only": position_count,
            "distinct_full_attribute_tuples_diagnostic_only": full_attribute_count,
            "distinct_position_tuples_are_not_smd_vertex_ids": True,
            "material_names": sorted(set(materials)),
            "source_influence_count_histogram": dict(sorted(hist.items())),
            "max_source_influences_per_record": max_inf,
            "records_with_more_than_four_influences": over4,
            "total_weight_discarded_by_runtime_importer_top_four_limit": discarded_weight,
            "max_global_inverse_identity_residual": max(matrix_residuals, default=0.0),
            "all_parent_references_valid": True, "all_time0_transforms_present": True,
            "all_influence_bone_ids_valid": True,
        },
        "runtime_bone_name_audit": {
            "exact_string_literals_from_name_is": runtime_names,
            "matched": runtime_matches, "missing_from_smd": missing_runtime,
            "name_truncation_collisions_at_31_utf8_bytes": truncated_collisions,
            "names_over_31_utf8_bytes": overlong_names,
        },
        "bones": out_bones,
    }
    (args.out / "skeleton_manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (args.out / "skeleton_hierarchy.txt").write_text("\n".join(hierarchy_lines) + "\n", encoding="utf-8")

    with (args.out / "skeleton_bones.csv").open("w", newline="", encoding="utf-8") as f:
        cols = ["id", "name", "parent_id", "parent_name", "tx", "ty", "tz", "rx", "ry", "rz"]
        for pre in ("local", "global", "inverse_bind"):
            cols += [f"{pre}_{i}" for i in range(12)]
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for b in out_bones:
            t = frames[0][b["id"]]
            row = {"id": b["id"], "name": b["name"], "parent_id": b["parent"],
                   "parent_name": nodes[b["parent"]]["name"] if b["parent"] >= 0 else "",
                   "tx": t[0], "ty": t[1], "tz": t[2], "rx": t[3], "ry": t[4], "rz": t[5]}
            for pre, key in (("local", "local_bind_3x4_row_major"),
                             ("global", "global_bind_3x4_row_major"),
                             ("inverse_bind", "inverse_bind_3x4_row_major")):
                row.update({f"{pre}_{i}": x for i, x in enumerate(b[key])})
            w.writerow(row)

    cols = ["record", "triangle", "corner", "material", "parent_bone", "x", "y", "z",
            "nx", "ny", "nz", "u", "v", "source_influence_count"]
    for k in range(max_inf):
        cols += [f"bone_id_{k+1}", f"weight_{k+1}"]
    with (args.out / "skin_weights.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for v in vertices:
            row = {"record": v["record"], "triangle": v["triangle"], "corner": v["corner"],
                   "material": v["material"], "parent_bone": v["parent_bone"],
                   "x": v["position"][0], "y": v["position"][1], "z": v["position"][2],
                   "nx": v["normal"][0], "ny": v["normal"][1], "nz": v["normal"][2],
                   "u": v["uv"][0], "v": v["uv"][1], "source_influence_count": len(v["influences"])}
            for k, inf in enumerate(v["influences"]):
                row[f"bone_id_{k+1}"], row[f"weight_{k+1}"] = inf["bone"], inf["weight"]
            w.writerow(row)

    report = [
        "# Naruto SMD rig audit", "",
        "Generated from the actual nested model archive by tools/audit_smd_rig.py.",
        "Read-only extraction: no gameplay, importer, or animation source was changed.", "",
        "## Source",
        f"- Outer ZIP SHA-256: {archive_sha}",
        f"- Nested ZIP: {nested[0]}",
        f"- Selected SMD: {selected}",
        f"- SMD SHA-256: {smd_sha}", "",
        "## Verified counts",
        f"- Bones: {len(ids)}; IDs {min(ids)} through {max(ids)}.",
        f"- Root IDs: {roots}.",
        f"- Skeleton frame times: {sorted(frames)}.",
        f"- Triangles: {len(vertices)//3}.",
        f"- SMD triangle-corner records: {len(vertices)}.",
        f"- Distinct position tuples (diagnostic only): {position_count}.",
        f"- Distinct complete attribute tuples (diagnostic only): {full_attribute_count}.",
        f"- Original influence histogram: {dict(sorted(hist.items()))}.",
        f"- Maximum original influences: {max_inf}.",
        f"- Records above runtime four-influence limit: {over4}.",
        f"- Parent links valid; all time-0 transforms present; all weight bone IDs valid: yes.",
        f"- Maximum global times inverse-bind identity residual: {max(matrix_residuals, default=0.0):.9g}.", "",
        "## Runtime name comparison",
        f"- Names used by current C pose code: {runtime_names}.",
        f"- Exact names matched in SMD: {[x['name'] for x in runtime_matches]}.",
        f"- Runtime names absent from SMD: {missing_runtime}.",
        f"- Name truncation collisions: {truncated_collisions}.",
        f"- Names longer than the importer's 31 UTF-8 byte limit: {overlong_names}.", "",
        "## Implementation facts",
        "- The C renderer calculates pose matrices in source coordinates and maps positions to (x,z,-y) after skinning.",
        "- The SMD file provides reference pose samples; actual combat clips are not established by this file.",
        "- The importer preserves source bone names up to 31 UTF-8 bytes and caps influences at four.",
        "- Position tuple counts are diagnostic only; SMD triangle records are not a reusable indexed vertex pool.", "",
        "## Output files",
        "- skeleton_manifest.json: all bone names, IDs, parent/children, source transforms and calculated matrices.",
        "- skeleton_hierarchy.txt: complete parent-child forest.",
        "- skeleton_bones.csv: transforms and matrices for each bone.",
        "- skin_weights.csv: every source triangle-corner record and all its source weights.",
    ]
    (args.out / "SKELETON_AUDIT.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    print("RIG AUDIT OK")
    print("SMD", selected)
    print("BONES", len(ids), "ROOTS", roots, "FRAMES", sorted(frames))
    print("TRIANGLES", len(vertices)//3, "TRIANGLE_CORNER_RECORDS", len(vertices))
    print("WEIGHT_HISTOGRAM", dict(sorted(hist.items())), "MAX_INFLUENCES", max_inf)
    print("RUNTIME_NAMES", runtime_names)
    print("MATCHED_RUNTIME_NAMES", [x["name"] for x in runtime_matches])
    print("MISSING_RUNTIME_NAMES", missing_runtime)
    print("MAX_INVERSE_RESIDUAL", max(matrix_residuals, default=0.0))
    print("OUTPUT_FILES", sorted(p.name for p in args.out.iterdir()))


if __name__ == "__main__":
    main()
