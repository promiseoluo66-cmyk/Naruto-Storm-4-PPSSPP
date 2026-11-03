#!/usr/bin/env python3
"""Read-only inventory of PSP assets embedded in extracted game archives.

This tool scans files without carving or modifying them. A signature match is
only a lead; only formats whose outer size can be checked are marked validated.
"""
from __future__ import annotations

import argparse
import csv
import json
import mmap
import os
import struct
import sys
from collections import Counter
from pathlib import Path
from typing import Any

SIGNATURES = (
    ("GMO", b"OMG.00.1PSP"),
    ("GIM", b"MIG.00.1PSP"),
    ("CPK", b"CPK "),
    ("RIFF", b"RIFF"),
    ("PNG", b"\x89PNG\r\n\x1a\n"),
    ("PSMF", b"PSMF"),
    ("ELF", b"\x7fELF"),
    ("ZIP", b"PK\x03\x04"),
    ("GZIP", b"\x1f\x8b"),
    ("PSP_PRX", b"~PSP"),
)

GMO_MAGIC = b"OMG.00.1PSP" + b"\x00" * 5


def inspect_gmo(data: mmap.mmap, pos: int) -> tuple[str, int | None, str]:
    """Validate the GMO file header and the outer File Start chunk."""
    remaining = len(data) - pos
    if remaining < 24 or data[pos:pos + 16] != GMO_MAGIC:
        return "signature-only", None, "incomplete or noncanonical GMO header"
    chunk_type, header_size, data_size = struct.unpack_from("<HHI", data, pos + 16)
    if chunk_type != 0x0002:
        return "header-inconsistent", None, f"first chunk type is 0x{chunk_type:04x}, expected 0x0002"
    if header_size < 8:
        return "header-inconsistent", None, f"chunk header size {header_size} is below 8"
    total = header_size + data_size
    if total < header_size or total > remaining - 16:
        return "header-inconsistent", None, "outer chunk size exceeds remaining bytes"
    return "validated-outer-chunk", 16 + total, "GMO magic and File Start chunk bounds are plausible"


def inspect_gim(data: mmap.mmap, pos: int) -> tuple[str, int | None, str]:
    """Check the GIM signature and root block header without assuming a carve size."""
    remaining = len(data) - pos
    if remaining < 32:
        return "signature-only", None, "GIM signature found but root block is incomplete"
    block_id, _unknown, block_size, next_offset, data_offset = struct.unpack_from("<HHIII", data, pos + 16)
    if block_id != 0x0002:
        return "header-inconsistent", None, f"first block id is 0x{block_id:04x}, expected root 0x0002"
    if block_size < 16 or block_size > remaining:
        return "header-inconsistent", None, "root block size is outside remaining bytes"
    if next_offset and next_offset > block_size:
        return "header-inconsistent", None, "root next-block offset exceeds root block size"
    if data_offset and data_offset > block_size:
        return "header-inconsistent", None, "root data offset exceeds root block size"
    # GIM block-size conventions vary in published descriptions; do not carve
    # from this value until a game-specific sample confirms its exact semantics.
    return "plausible-root-block", None, f"root id valid; declared block_size={block_size}"


def inspect_riff(data: mmap.mmap, pos: int) -> tuple[str, int | None, str]:
    remaining = len(data) - pos
    if remaining < 12:
        return "signature-only", None, "RIFF header is incomplete"
    declared = struct.unpack_from("<I", data, pos + 4)[0] + 8
    if declared < 12 or declared > remaining:
        return "header-inconsistent", declared, "RIFF declared length exceeds remaining bytes"
    kind = bytes(data[pos + 8:pos + 12]).decode("ascii", "replace")
    return "validated-length", declared, f"RIFF form={kind}"


def inspect_png(data: mmap.mmap, pos: int) -> tuple[str, int | None, str]:
    remaining = len(data) - pos
    if remaining < 33:
        return "signature-only", None, "PNG header is incomplete"
    cursor = pos + 8
    while cursor + 12 <= len(data):
        length = struct.unpack_from(">I", data, cursor)[0]
        end = cursor + 12 + length
        if end > len(data):
            return "header-inconsistent", None, "PNG chunk extends past containing file"
        chunk_type = bytes(data[cursor + 4:cursor + 8])
        if chunk_type == b"IEND":
            return "validated-length", end - pos, "PNG chunk sequence reaches IEND"
        cursor = end
        if cursor - pos > remaining:
            break
    return "signature-only", None, "PNG signature found; no complete IEND before file end"


def inspect_hit(data: mmap.mmap, pos: int, label: str) -> tuple[str, int | None, str]:
    if label == "GMO":
        return inspect_gmo(data, pos)
    if label == "GIM":
        return inspect_gim(data, pos)
    if label == "RIFF":
        return inspect_riff(data, pos)
    if label == "PNG":
        return inspect_png(data, pos)
    if label == "ELF":
        if pos + 16 <= len(data) and data[pos + 4] in (1, 2) and data[pos + 5] in (1, 2):
            return "plausible-header", None, "ELF class and byte-order fields are recognised"
        return "signature-only", None, "ELF magic found; header fields are incomplete or unusual"
    return "signature-only", None, "signature match only; container boundary not established"


def scan_file(path: Path, root: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    size = path.stat().st_size
    if size == 0:
        return rows
    with path.open("rb") as stream:
        with mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
            for label, signature in SIGNATURES:
                offset = 0
                while True:
                    pos = data.find(signature, offset)
                    if pos < 0:
                        break
                    status, declared, detail = inspect_hit(data, pos, label)
                    rows.append({
                        "file": path.relative_to(root).as_posix(),
                        "file_bytes": size,
                        "offset": pos,
                        "offset_hex": f"0x{pos:X}",
                        "signature": label,
                        "status": status,
                        "declared_bytes": declared if declared is not None else "",
                        "detail": detail,
                    })
                    # Advance one byte so overlapping signatures are not skipped.
                    offset = pos + 1
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Read-only scan of extracted PSP archive files; does not extract or modify assets."
    )
    parser.add_argument("root", type=Path, help="folder containing extracted archive entries")
    parser.add_argument("--csv", dest="csv_path", type=Path, default=Path("impact-asset-report.csv"))
    parser.add_argument("--json", dest="json_path", type=Path, default=None)
    args = parser.parse_args()

    root = args.root.expanduser().resolve()
    if not root.is_dir():
        parser.error(f"input folder does not exist: {root}")

    rows: list[dict[str, Any]] = []
    scanned = 0
    errors: list[dict[str, str]] = []
    for directory, dirnames, filenames in os.walk(root, followlinks=False):
        dirnames[:] = sorted(d for d in dirnames if not (Path(directory) / d).is_symlink())
        for filename in sorted(filenames):
            path = Path(directory) / filename
            if path.is_symlink() or not path.is_file():
                continue
            scanned += 1
            try:
                rows.extend(scan_file(path, root))
            except (OSError, ValueError) as exc:
                errors.append({"file": str(path.relative_to(root)), "error": str(exc)})

    args.csv_path.parent.mkdir(parents=True, exist_ok=True)
    fields = ["file", "file_bytes", "offset", "offset_hex", "signature", "status", "declared_bytes", "detail"]
    with args.csv_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)

    if args.json_path:
        args.json_path.parent.mkdir(parents=True, exist_ok=True)
        args.json_path.write_text(json.dumps({"root": str(root), "files_scanned": scanned,
                                              "matches": rows, "errors": errors}, indent=2),
                                  encoding="utf-8")

    by_type = Counter(row["signature"] for row in rows)
    by_status = Counter(row["status"] for row in rows)
    print(f"Files scanned: {scanned}")
    print(f"Signature matches: {len(rows)}")
    for key, count in sorted(by_type.items()):
        print(f"  {key}: {count}")
    print("Validation:")
    for key, count in sorted(by_status.items()):
        print(f"  {key}: {count}")
    print(f"CSV report: {args.csv_path.expanduser()}")
    if args.json_path:
        print(f"JSON report: {args.json_path.expanduser()}")
    if errors:
        print(f"Unreadable files: {len(errors)} (see JSON report when enabled)")
    print("No files were extracted, changed, or deleted.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
