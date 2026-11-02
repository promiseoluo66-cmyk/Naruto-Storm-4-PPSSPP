# Impact PGD decryptor (first extraction stage)

This tool decrypts one raw PSP PGD file. It is the first stage of the
Ultimate Ninja Impact asset path; it is not a DIG archive extractor and
does not yet export models, skeletons, animations, or movesets.

## Why this implementation

The implementation follows PPSSPP's open-source PGD read path:

1. Initialise the KIRK crypto engine.
2. Call pgd_open(..., 2, nullptr) — PPSSPP's sceIo PGD data-read path uses
   open flag 2.
3. Read encrypted blocks from the decrypted descriptor's data_offset.
4. Call pgd_decrypt_block() for each block and write exactly data_size
   bytes of plaintext.

The build downloads PPSSPP at pinned commit
fc9ca4d1f78a5d60ae03f3311c6b8ab49273f6a8 and builds its ext/libkirk
sources. prepare_build.py makes a build-time copy and adds bounds checks
around the decrypted PGD descriptor's arithmetic and block allocation. The
upstream checkout itself is not edited.

PPSSPP source: https://github.com/hrydgard/ppsspp
PGD implementation: ext/libkirk/amctrl.c, ext/libkirk/amctrl.h, and
Core/HLE/sceIo.cpp.

## Build on Android / Termux

The script is intended for the user's Android Termux environment:

    pkg install git cmake clang make python -y
    bash tools/impact-pgd/build_termux.sh

It builds a native executable for the device. It does not need the ISO in
GitHub or a cloud build runner.

## Decrypt one of the Impact PGD entries directly from the ISO

The four candidate files are inside the ISO, so you do not need to extract
them manually. First list the candidates:

    python3 tools/impact-pgd/decrypt_impact_iso.py \
      "$HOME/storage/downloads/2736 - Naruto Shippuden - Ultimate Ninja Impact (USA) (En,Fr,Es).iso" \
      --list-candidates

Then decrypt one candidate. Start with 0012.BIN because it is the largest
candidate and is the best first test of the main archive path:

    python3 tools/impact-pgd/decrypt_impact_iso.py \
      "$HOME/storage/downloads/2736 - Naruto Shippuden - Ultimate Ninja Impact (USA) (En,Fr,Es).iso" \
      --entry PSP_GAME/INSDIR/0012.BIN \
      --output "$HOME/storage/downloads/impact-0012-decrypted.bin"

The wrapper copies only that one ISO member into temporary storage, invokes
the C++ PGD decryptor, and removes the temporary copy when finished. The ISO
and decrypted output remain on the phone; neither is uploaded to GitHub.
Use --force only when you intentionally want to replace an existing output.

The output is written only after all blocks decrypt successfully. The command
prints a JSON summary including the PGD descriptor values, first plaintext
bytes, and a conservative format hint. If the output path already exists, the
tool refuses to overwrite it unless --force is supplied.

## How to interpret results

- "pgd_open rejected the header": PGD MAC/key validation failed. This may be
  the wrong key mode or a different PGD variant; it is not a successful
  decryption.
- "status: pgd-decrypted": PPSSPP's PGD authentication and block decryption
  completed and a plaintext file was written.
- "format_hint: unknown": decryption may still have worked. The first bytes
  must then be checked against NNL's DIG detector; do not rename it to
  CFC.DIG based only on its filename.
- "CFC.DIG signature candidate": a promising header match, but NNL/Racjin
  format detection is still required.


## Validate DIG structure and scan all four candidates

NNL identifies a DIG archive by validating its record table and all referenced
payload ranges, not by looking for a literal CFC.DIG string. The local
dig_probe.py script ports those structural checks:

    python3 tools/impact-pgd/dig_probe.py /path/to/decrypted.bin

To attempt all four known PGD entries in one pass:

    python3 tools/impact-pgd/probe_all_impact_pgds.py \
      "$HOME/storage/downloads/2736 - Naruto Shippuden - Ultimate Ninja Impact (USA) (En,Fr,Es).iso"

The batch scanner:
- processes the four candidates sequentially to limit peak storage;
- authenticates and decrypts each PGD with the compiled decryptor;
- validates the plaintext against the NNL DIG record-table rules;
- preserves outputs that pass DIG validation in the results folder;
- deletes decrypted outputs that do not pass validation by default;
- writes impact-pgd-scan-report.json with a result for each candidate.

Use --keep-unknown only when you intentionally want to retain decrypted
non-DIG files for manual inspection. Use --force to replace existing results.
A structurally valid DIG file is not yet a character export; it is the input
to the next archive-entry and asset-enumeration stage.

## Test status and limitations

--self-test and CTest check the CLI's local validation logic. They do not
prove that any of the user's real PGD files decrypt. A real-file test is still
required before claiming the Impact archive stage works.

This stage does not attempt to bypass an unknown key, guess keys, decrypt
unrelated DRM files, unpack DIG archives, or produce character assets.

## Licensing

The decryptor links PPSSPP's libkirk code at build time. PPSSPP is GPL-2.0-or-later;
the included libkirk files carry their own license notices (including GPL-3.0-or-later
notices in some files). See the upstream source and retain its notices when
redistributing binaries or modified source.
