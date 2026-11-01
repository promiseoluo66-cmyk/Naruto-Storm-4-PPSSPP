#!/usr/bin/env python3
"""Read-only ISO9660 reconnaissance for Naruto: Narutimate Accel 3.

This tool inventories a locally-owned PSP ISO. It does not modify the ISO,
decrypt game content, or export copyrighted models/audio/animations.
Only standard-library Python is required.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path
from typing import BinaryIO

SECTOR = 2048
MAX_ENTRIES = 200_000
GAME_IDS = (b"ULJS00236", b"ULJS-00236", b"ULUS10518", b"ULUS-10518")

MAGIC = (
    (b"\x7fELF", "ELF executable"),
    (b"PSP\x00", "PSP marker"),
    (b"PBP\x00", "PSP PBP"),
    (b"CISO", "Compressed ISO"),
    (b"ZISO", "Compressed ISO (ZISO)"),
    (b"PK\x03\x04", "ZIP"),
    (b"7z\xbc\xaf\x27\x1c", "7-Zip"),
    (b"Rar!\x1a\x07", "RAR"),
    (b"\x1f\x8b", "GZIP"),
    (b"CPK ", "CRI CPK"),
    (b"CRILAYLA", "CRI-LAYLA"),
    (b"XFBIN", "CyberConnect2 XFBIN"),
    (b"DDS ", "DDS texture"),
    (b"TIM2", "TIM2 texture"),
    (b"MIG.", "Possible MIG container"),
    (b"OggS", "Ogg audio"),
    (b"RIFF", "RIFF media/container"),
    (b"PSMF", "PSP movie stream"),
)

def u16le(data: bytes, off: int) -> int:
    return struct.unpack_from("<H", data, off)[0]

def u32le(data: bytes, off: int) -> int:
    return struct.unpack_from("<I", data, off)[0]

def read_at(f: BinaryIO, offset: int, size: int) -> bytes:
    f.seek(offset)
    return f.read(size)

def detect_magic(head: bytes) -> str:
    for magic, label in MAGIC:
        if head.startswith(magic):
            return label
    return "unknown / game-specific"

def parse_record(rec: bytes) -> dict | None:
    if len(rec) < 34 or rec[0] < 34:
        return None
    name_len = rec[32]
    if 33 + name_len > len(rec):
        return None
    raw_name = rec[33:33 + name_len]
    if raw_name == b"\x00":
        name = "."
    elif raw_name == b"\x01":
        name = ".."
    else:
        name = raw_name.decode("ascii", errors="replace")
        if ";" in name:
            name = name.rsplit(";", 1)[0]
        name = name.rstrip(".")
    return {
        "name": name,
        "extent": u32le(rec, 2),
        "size": u32le(rec, 10),
        "directory": bool(rec[25] & 0x02),
        "flags": rec[25],
    }

def read_directory(f: BinaryIO, extent: int, size: int) -> list[dict]:
    base = extent * SECTOR
    raw = read_at(f, base, size)
    entries: list[dict] = []
    pos = 0
    while pos < len(raw):
        record_len = raw[pos]
        if record_len == 0:
            pos = ((pos // SECTOR) + 1) * SECTOR
            continue
        if record_len < 34 or pos + record_len > len(raw):
            break
        item = parse_record(raw[pos:pos + record_len])
        if item and item["name"] not in (".", ".."):
            entries.append(item)
        pos += record_len
    return entries

def find_primary_volume(f: BinaryIO, file_size: int) -> tuple[int, dict]:
    for lba in range(16, 64):
        offset = lba * SECTOR
        if offset + SECTOR > file_size:
            break
        block = read_at(f, offset, SECTOR)
        if len(block) < SECTOR or block[1:6] != b"CD001":
            continue
        if block[0] == 1:
            root_len = block[156]
            root = parse_record(block[156:156 + root_len])
            if not root or not root["directory"]:
                raise ValueError("ISO9660 PVD found, but its root directory record is invalid.")
            logical_block_size = u16le(block, 128)
            if logical_block_size != SECTOR:
                raise ValueError(f"Unsupported ISO logical block size: {logical_block_size}")
            return lba, root
        if block[0] == 255:
            break
    raise ValueError("No ISO9660 Primary Volume Descriptor found. The image may be a CSO, encrypted image, or another format.")

def inventory_iso(path: Path) -> dict:
    file_size = path.stat().st_size
    rows: list[dict] = []
    seen_dirs: set[tuple[int, int]] = set()
    with path.open("rb") as f:
        pvd_lba, root = find_primary_volume(f, file_size)
        stack: list[tuple[str, dict]] = [("", root)]
        while stack:
            parent, directory = stack.pop()
            marker = (directory["extent"], directory["size"])
            if marker in seen_dirs:
                continue
            seen_dirs.add(marker)
            for item in read_directory(f, directory["extent"], directory["size"]):
                name = item["name"]
                rel = f"{parent}/{name}" if parent else name
                entry = {
                    "path": rel,
                    "size": item["size"],
                    "extent_lba": item["extent"],
                    "directory": item["directory"],
                }
                if item["directory"]:
                    entry["type"] = "directory"
                    rows.append(entry)
                    stack.append((rel, item))
                else:
                    head = read_at(f, item["extent"] * SECTOR, min(item["size"], 64))
                    entry["type"] = detect_magic(head)
                    entry["first_bytes_hex"] = head[:16].hex()
                    rows.append(entry)
                if len(rows) > MAX_ENTRIES:
                    raise ValueError(f"Entry limit ({MAX_ENTRIES}) exceeded; stopping safely.")
        rows.sort(key=lambda x: x["path"].lower())

        psf = next((r for r in rows if r["path"].upper().endswith("/PARAM.SFO")), None)
        game_id_hits = []
        if psf:
            content = read_at(f, psf["extent_lba"] * SECTOR, min(psf["size"], 1024 * 1024))
            game_id_hits = [gid.decode("ascii") for gid in GAME_IDS if gid in content]

    candidates = [
        r for r in rows if not r["directory"] and (
            r["type"] == "unknown / game-specific"
            or Path(r["path"]).suffix.lower() in {".xxx", ".bin", ".dat", ".pak", ".arc", ".cpk", ".gmo", ".gim"}
        )
    ]
    return {
        "tool": "accel3-iso-probe",
        "tool_version": "0.1.0",
        "input_file_name": path.name,
        "input_size_bytes": file_size,
        "iso9660_primary_volume_descriptor_lba": pvd_lba,
        "expected_title_ids": ["ULJS00236 (Japan/Korea)", "ULUS10518 (USA)"],
        "param_sfo_game_id_matches": game_id_hits,
        "entry_count": len(rows),
        "candidate_game_specific_files": candidates,
        "entries": rows,
        "limitations": [
            "Read-only inventory only; no game archive decryption or unpacking.",
            "Unknown signatures are not proof of a proprietary format.",
            "No model, skeleton, animation, moveset, texture, or audio payload is exported.",
            "A format parser must be written and validated after candidate files are examined."
        ],
    }

def main() -> int:
    parser = argparse.ArgumentParser(description="Inventory a PSP ISO locally without uploading game data.")
    parser.add_argument("iso", type=Path, help="Path to your locally stored .iso file")
    parser.add_argument("--out", type=Path, default=Path("accel3-report.json"), help="Output JSON report")
    args = parser.parse_args()
    if not args.iso.is_file():
        parser.error(f"Input file does not exist: {args.iso}")
    try:
        report = inventory_iso(args.iso)
        args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
    except (OSError, ValueError, struct.error) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    print(f"Report written: {args.out}")
    print(f"ISO entries: {report['entry_count']}")
    print(f"Likely game-specific files: {len(report['candidate_game_specific_files'])}")
    if report["param_sfo_game_id_matches"]:
        print("Detected game ID: " + ", ".join(report["param_sfo_game_id_matches"]))
    else:
        print("Game ID not found in PARAM.SFO scan; check the image/region manually.")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
