#!/usr/bin/env python3
"""Structural validator for the primary CFC.DIG / NSUNI .BIN archive.

This mirrors the public NNL dig::IsOfType checks. DIG has no fixed magic
signature, so this validates the record table and every referenced payload.
"""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

BLOCK_SIZE = 0x800
RECORD = struct.Struct("<IIHHI")
MAX_RECORDS = 1_000_000
MAX_OFFSET = 0xFFFFFFFF // BLOCK_SIZE


def record_is_valid(offset: int, compressed_size: int, num_entries: int,
                    is_compressed: int, decompressed_size: int) -> bool:
    if is_compressed > 1 or offset > MAX_OFFSET:
        return False
    if offset != 0 and (
        compressed_size == 0
        or decompressed_size == 0
        or num_entries == 0
        or compressed_size > decompressed_size
    ):
        return False
    if not is_compressed and compressed_size != decompressed_size:
        return False
    if is_compressed and (
        offset == 0 or compressed_size == 0 or decompressed_size == 0
    ):
        return False
    return True


def probe_dig(path: Path) -> dict:
    size = path.stat().st_size
    result = {
        "format": "DIG",
        "valid": False,
        "file_size": size,
        "record_count": 0,
        "reason": "",
        "validator": "NNL dig::IsOfType structural checks (Python port)",
    }
    if size <= BLOCK_SIZE:
        result["reason"] = "file is not larger than the DIG block size"
        return result

    with path.open("rb") as f:
        first_nonzero_offset = None
        scanned = 0
        while f.tell() + RECORD.size <= size:
            raw = f.read(RECORD.size)
            scanned += 1
            if len(raw) != RECORD.size:
                result["reason"] = "truncated record while estimating table length"
                return result
            rec = RECORD.unpack(raw)
            if not record_is_valid(*rec):
                result["reason"] = f"invalid record while estimating table length at index {scanned - 1}"
                return result
            if rec[0] != 0:
                first_nonzero_offset = rec[0]
                break
            if scanned >= MAX_RECORDS:
                result["reason"] = "record scan limit reached"
                return result

        if first_nonzero_offset is None:
            result["reason"] = "no non-empty payload record found"
            return result

        record_count = (first_nonzero_offset * BLOCK_SIZE) // RECORD.size
        result["record_count"] = record_count
        if record_count == 0 or record_count > MAX_RECORDS:
            result["reason"] = "record count is zero or exceeds the safety limit"
            return result
        table_size = record_count * RECORD.size
        if size <= table_size:
            result["reason"] = "file ends before or at the end of its record table"
            return result

        f.seek(0)
        for index in range(record_count):
            raw = f.read(RECORD.size)
            if len(raw) != RECORD.size:
                result["reason"] = f"truncated record at index {index}"
                return result
            rec = RECORD.unpack(raw)
            if not record_is_valid(*rec):
                result["reason"] = f"invalid record at index {index}"
                return result
            offset, compressed_size, _num_entries, _compressed, _decompressed = rec
            end = offset * BLOCK_SIZE + compressed_size
            if end > size:
                result["reason"] = f"record {index} payload extends beyond the file"
                return result

    result["valid"] = True
    result["reason"] = "record table and all referenced payload ranges are structurally valid"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Check whether a decrypted file is a NNL-compatible DIG archive.")
    parser.add_argument("file", type=Path)
    parser.add_argument("--out", type=Path, help="Optional JSON report path")
    args = parser.parse_args()
    try:
        result = probe_dig(args.file)
    except OSError as exc:
        parser.error(str(exc))
    rendered = json.dumps(result, indent=2)
    print(rendered)
    if args.out:
        args.out.write_text(rendered + "\n", encoding="utf-8")
    return 0 if result["valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
