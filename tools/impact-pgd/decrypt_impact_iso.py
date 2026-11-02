#!/usr/bin/env python3
"""Extract one PGD member from a local Impact ISO and pass it to the decryptor."""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOOLS_DIR / "accel3-recon"))

from accel3_probe import inventory_iso  # noqa: E402

PGD_PREFIX = "00504744"
KNOWN_IMPACT_PGD_ENTRIES = (
    "PSP_GAME/INSDIR/0005.BIN",
    "PSP_GAME/INSDIR/0007.BIN",
    "PSP_GAME/INSDIR/0008.BIN",
    "PSP_GAME/INSDIR/0012.BIN",
)
SECTOR_SIZE = 2048


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Decrypt a PGD file directly from a local Ultimate Ninja Impact ISO."
    )
    parser.add_argument("iso", type=Path, help="Path to the local Ultimate Ninja Impact ISO")
    parser.add_argument("--entry", help="ISO member path, e.g. PSP_GAME/INSDIR/0012.BIN")
    parser.add_argument("--list-candidates", action="store_true", help="List the four previously identified PGD candidates")
    parser.add_argument(
        "--exe",
        type=Path,
        default=PROJECT_ROOT / "tools" / "impact-pgd" / "build" / "impact_pgd_decryptor",
        help="Path to the compiled impact_pgd_decryptor executable",
    )
    parser.add_argument("--output", type=Path, help="Destination for decrypted plaintext")
    parser.add_argument("--force", action="store_true", help="Replace an existing output file")
    args = parser.parse_args()

    if not args.iso.is_file():
        parser.error(f"ISO does not exist: {args.iso}")

    try:
        report = inventory_iso(args.iso)
    except (OSError, ValueError) as exc:
        print(f"ERROR: could not inventory ISO: {exc}", file=sys.stderr)
        return 2

    by_path = {row["path"].casefold(): row for row in report["entries"] if not row.get("directory")}
    if args.list_candidates:
        for known in KNOWN_IMPACT_PGD_ENTRIES:
            row = by_path.get(known.casefold())
            if row:
                is_pgd = row.get("first_bytes_hex", "").lower().startswith(PGD_PREFIX)
                print(
                    f"{row['path']}\tsize={row['size']}\t"
                    f"lba={row['extent_lba']}\tpgd_magic={is_pgd}"
                )
            else:
                print(f"{known}\tnot found")
        return 0

    if not args.entry or not args.output:
        parser.error("--entry and --output are required unless --list-candidates is used")

    row = by_path.get(args.entry.replace("\\", "/").casefold())
    if not row:
        print(f"ERROR: ISO member not found: {args.entry}", file=sys.stderr)
        return 2

    expected_magic = row.get("first_bytes_hex", "").lower()
    if not expected_magic.startswith(PGD_PREFIX):
        print(
            f"ERROR: {row['path']} does not begin with the expected raw PGD magic; "
            f"first_bytes_hex={expected_magic}",
            file=sys.stderr,
        )
        return 3

    if not args.exe.is_file():
        print(f"ERROR: decryptor executable not found: {args.exe}", file=sys.stderr)
        print("Build it first with: bash tools/impact-pgd/build_termux.sh", file=sys.stderr)
        return 2
    if args.output.exists() and not args.force:
        print(f"ERROR: output already exists: {args.output} (use --force to replace it)", file=sys.stderr)
        return 2

    source_offset = int(row["extent_lba"]) * SECTOR_SIZE
    source_size = int(row["size"])
    temp_parent = Path(tempfile.gettempdir())
    free_bytes = shutil.disk_usage(temp_parent).free
    if free_bytes < source_size + 32 * 1024 * 1024:
        print(
            f"ERROR: not enough temporary space. Need about {source_size / (1024 * 1024):.1f} MiB "
            f"plus working space; free={free_bytes / (1024 * 1024):.1f} MiB",
            file=sys.stderr,
        )
        return 2

    # The current C++ interface accepts a standalone PGD file. Extract only
    # this one ISO member to private temporary storage, then remove it on exit.
    with tempfile.TemporaryDirectory(prefix="impact-pgd-") as temp_dir:
        pgd_path = Path(temp_dir) / "candidate.pgd"
        remaining = source_size
        with args.iso.open("rb") as source, pgd_path.open("wb") as destination:
            source.seek(source_offset)
            while remaining:
                chunk = source.read(min(1024 * 1024, remaining))
                if not chunk:
                    print("ERROR: ISO ended while reading the selected member", file=sys.stderr)
                    return 5
                destination.write(chunk)
                remaining -= len(chunk)

        command = [str(args.exe), str(pgd_path), str(args.output)]
        if args.force:
            command.append("--force")
        print(f"Selected: {row['path']} ({source_size} bytes)")
        print("Temporary ISO member will be removed after the decryptor exits.")
        result = subprocess.run(command, check=False)
        return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
