#!/usr/bin/env python3
"""Copy PPSSPP libkirk and harden PGD descriptor arithmetic before compiling it."""
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path


PATCH = re.compile(
    r"(?m)^[ \t]*pgd->align_size\s*=.*\n"
    r"[ \t]*pgd->table_offset\s*=.*\n"
    r"[ \t]*pgd->block_nr\s*=.*\n"
    r"[ \t]*pgd->block_nr\s*=.*"
)

REPLACEMENT = """\t// Validate encrypted descriptor values before calculating sizes or allocating.
\tif (pgd->data_size == 0u || !pgd_valid_block_size(pgd->block_size)) {
\t\tfree(pgd);
\t\treturn NULL;
\t}
\tconst uint64_t aligned_size = ((uint64_t)pgd->data_size + 15u) & ~(uint64_t)15u;
\tconst uint64_t block_count = (aligned_size + pgd->block_size - 1u) / pgd->block_size;
\tconst uint64_t block_span = block_count * pgd->block_size;
\tif (aligned_size > UINT32_MAX || block_count > UINT32_MAX ||
\t\tblock_span > UINT32_MAX ||
\t\t(uint64_t)pgd->data_offset + block_span > UINT32_MAX) {
\t\tfree(pgd);
\t\treturn NULL;
\t}
\tpgd->align_size = (u32)aligned_size;
\tpgd->table_offset = pgd->data_offset + pgd->align_size;
\tpgd->block_nr = (u32)block_count"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ppsspp-src", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    source_dir = args.ppsspp_src / "ext" / "libkirk"
    if not (source_dir / "amctrl.c").is_file():
        parser.error(f"PPSSPP libkirk source not found under {source_dir}")

    if args.output_dir.exists():
        shutil.rmtree(args.output_dir)
    shutil.copytree(source_dir, args.output_dir)

    amctrl = args.output_dir / "amctrl.c"
    text = amctrl.read_text(encoding="utf-8")
    if "#include <stdint.h>" not in text:
        anchor = "#include <string.h>"
        if anchor not in text:
            raise SystemExit("Could not find stable include anchor in PPSSPP amctrl.c")
        text = text.replace(anchor, anchor + "\n#include <stdint.h>", 1)

    patched, count = PATCH.subn(REPLACEMENT, text, count=1)
    if count != 1:
        raise SystemExit(
            "Could not apply the expected PGD descriptor bounds patch. "
            "The pinned PPSSPP source may have changed; refusing an unsafe build."
        )
    amctrl.write_text(patched, encoding="utf-8", newline="\n")
    print(f"Prepared PPSSPP libkirk in {args.output_dir}")
    print("Applied bounds checks to PGD descriptor arithmetic/allocation.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
