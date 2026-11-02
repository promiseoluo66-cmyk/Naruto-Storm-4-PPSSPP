#!/usr/bin/env python3
"""Try every known Impact PGD candidate and keep only structurally valid DIG files."""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOOLS_DIR / "accel3-recon"))

from accel3_probe import inventory_iso  # noqa: E402
from dig_probe import probe_dig  # noqa: E402

KNOWN_CANDIDATES = (
    "PSP_GAME/INSDIR/0005.BIN",
    "PSP_GAME/INSDIR/0007.BIN",
    "PSP_GAME/INSDIR/0008.BIN",
    "PSP_GAME/INSDIR/0012.BIN",
)
SECTOR_SIZE = 2048
PGD_MAGIC_HEX = "00504744"


def extract_extent(iso_path: Path, row: dict, destination: Path) -> None:
    remaining = int(row["size"])
    offset = int(row["extent_lba"]) * SECTOR_SIZE
    with iso_path.open("rb") as source, destination.open("wb") as target:
        source.seek(offset)
        while remaining:
            chunk = source.read(min(1024 * 1024, remaining))
            if not chunk:
                raise OSError("ISO ended before the candidate file was fully read")
            target.write(chunk)
            remaining -= len(chunk)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Try all four known Ultimate Ninja Impact PGD files and identify DIG archives."
    )
    parser.add_argument("iso", type=Path)
    parser.add_argument(
        "--exe",
        type=Path,
        default=PROJECT_ROOT / "tools" / "impact-pgd" / "build" / "impact_pgd_decryptor",
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path.home() / "storage" / "downloads" / "impact-pgd-results",
    )
    parser.add_argument("--keep-unknown", action="store_true",
                        help="Keep decrypted outputs that do not pass DIG validation")
    parser.add_argument("--force", action="store_true",
                        help="Replace existing final output files and report")
    args = parser.parse_args()

    if not args.iso.is_file():
        parser.error(f"ISO does not exist: {args.iso}")
    if not args.exe.is_file():
        parser.error(
            f"Decryptor not found: {args.exe}. Build first with "
            "bash tools/impact-pgd/build_termux.sh"
        )
    args.out_dir.mkdir(parents=True, exist_ok=True)
    report_path = args.out_dir / "impact-pgd-scan-report.json"
    if report_path.exists() and not args.force:
        parser.error(f"Report already exists: {report_path}; use --force or choose another --out-dir")

    try:
        inventory = inventory_iso(args.iso)
    except (OSError, ValueError) as exc:
        print(f"ERROR: cannot inventory ISO: {exc}", file=sys.stderr)
        return 2

    entries = {
        row["path"].casefold(): row
        for row in inventory["entries"]
        if not row.get("directory")
    }
    results = []
    success_count = 0

    for candidate in KNOWN_CANDIDATES:
        row = entries.get(candidate.casefold())
        item = {"entry": candidate, "status": "not-found"}
        if row is None:
            results.append(item)
            print(f"[SKIP] {candidate}: not found in this ISO")
            continue

        item["size_bytes"] = int(row["size"])
        item["extent_lba"] = int(row["extent_lba"])
        first_bytes = row.get("first_bytes_hex", "").lower()
        if not first_bytes.startswith(PGD_MAGIC_HEX):
            item["status"] = "not-pgd"
            item["first_bytes_hex"] = first_bytes
            results.append(item)
            print(f"[SKIP] {candidate}: expected PGD magic was not present")
            continue

        final_path = args.out_dir / (Path(candidate).stem + ".decrypted.DIG.bin")
        unknown_path = args.out_dir / (Path(candidate).stem + ".decrypted.unknown.bin")
        if (final_path.exists() or unknown_path.exists()) and not args.force:
            item["status"] = "output-exists"
            item["message"] = "Use --force or a different --out-dir"
            results.append(item)
            print(f"[SKIP] {candidate}: output already exists")
            continue

        size = int(row["size"])
        temp_free = shutil.disk_usage(Path(tempfile.gettempdir())).free
        out_free = shutil.disk_usage(args.out_dir).free
        required = size + 16 * 1024 * 1024
        if temp_free < required or out_free < required:
            item["status"] = "insufficient-space"
            item["temp_free_bytes"] = temp_free
            item["output_free_bytes"] = out_free
            results.append(item)
            print(f"[SKIP] {candidate}: insufficient free storage for safe temporary processing")
            continue

        stage_path = args.out_dir / (".working-" + Path(candidate).stem + ".bin")
        if stage_path.exists():
            stage_path.unlink()

        try:
            with tempfile.TemporaryDirectory(prefix="impact-pgd-candidate-") as temp_dir:
                raw_path = Path(temp_dir) / "candidate.pgd"
                extract_extent(args.iso, row, raw_path)
                proc = subprocess.run(
                    [str(args.exe), str(raw_path), str(stage_path)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
            item["decrypt_exit_code"] = proc.returncode
            if proc.stdout.strip():
                try:
                    item["decryptor"] = json.loads(proc.stdout)
                except json.JSONDecodeError:
                    item["decryptor_stdout"] = proc.stdout[-3000:]
            if proc.stderr.strip():
                item["decryptor_error"] = proc.stderr[-3000:]

            if proc.returncode != 0 or not stage_path.is_file():
                item["status"] = "pgd-decryption-failed"
                results.append(item)
                print(f"[FAIL] {candidate}: PGD authentication/decryption did not complete")
                continue

            validation = probe_dig(stage_path)
            item["dig_validation"] = validation
            if validation["valid"]:
                if args.force and final_path.exists():
                    final_path.unlink()
                stage_path.replace(final_path)
                item["status"] = "dig-archive"
                item["output"] = str(final_path)
                success_count += 1
                print(
                    f"[DIG] {candidate}: validated {validation['record_count']} records -> {final_path}"
                )
            elif args.keep_unknown:
                if args.force and unknown_path.exists():
                    unknown_path.unlink()
                stage_path.replace(unknown_path)
                item["status"] = "decrypted-non-dig"
                item["output"] = str(unknown_path)
                print(f"[UNKNOWN] {candidate}: decrypted, but did not validate as DIG")
            else:
                stage_path.unlink(missing_ok=True)
                item["status"] = "decrypted-but-not-dig"
                print(f"[UNKNOWN] {candidate}: decryption succeeded, but output was not DIG; discarded")
        except (OSError, subprocess.SubprocessError) as exc:
            stage_path.unlink(missing_ok=True)
            item["status"] = "processing-error"
            item["error"] = str(exc)
            print(f"[ERROR] {candidate}: {exc}", file=sys.stderr)
        finally:
            stage_path.unlink(missing_ok=True)

        results.append(item)

    report = {
        "tool": "impact-pgd-batch-probe",
        "tool_version": "0.1.0",
        "input_iso": str(args.iso),
        "candidate_count": len(KNOWN_CANDIDATES),
        "dig_archives_found": success_count,
        "results": results,
        "limitations": [
            "A DIG archive passing structural validation is not yet a character export.",
            "The next stage is DIG entry decompression/unpacking and NNL asset enumeration.",
            "Candidate PGD failures may indicate a different PGD key mode or non-asset PGD content."
        ]
    }
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"\nReport: {report_path}")
    print(f"Validated DIG archives: {success_count}")
    return 0 if success_count else 1


if __name__ == "__main__":
    raise SystemExit(main())
