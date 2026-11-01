# Narutimate Accel 3 (Japan) — format reconnaissance

Target: Naruto Shippuden: Narutimate Accel 3, Japan/Korea PSP release,
game ID ULJS-00236. The regional English release is Ultimate Ninja Heroes 3,
game ID ULUS-10518. This branch prioritizes the Japanese release.

## What the research established

- The Japanese release is identified as ULJS-00236; its disc dump is reported as
  a 644,841,472-byte ISO with CRC32 674025ae.
- Existing public PSP modding material found for this exact title mainly documents
  PPSSPP texture replacement. That is not a 3D model or animation extractor.
- UNIT/NNL target Ultimate Ninja Impact and Legends: Akatsuki Rising, not Accel 3.
- CyberConnect2 XFBIN tools found during research target later Storm/XFBIN games;
  no public evidence found that they parse Accel 3's PSP asset containers.
- Therefore, we must not label UNIT, NNL, or XFBIN importers as compatible with Accel 3.

## Stage started in this branch

accel3_probe.py is a read-only Python 3 ISO9660 inventory tool. It:
1. Lists paths, file sizes, and ISO sector offsets.
2. Inspects the first bytes of files for known container signatures.
3. Checks PSP_GAME/PARAM.SFO for the Japanese/US game IDs.
4. Creates a JSON report identifying unknown or likely game-specific files.

It does not decrypt or unpack the game's proprietary containers, nor export
models, skeletons, animation clips, or moveset/action tables. The report is the
evidence needed to choose and implement the next parser, rather than guessing.

## Run locally on Android (Termux) or desktop

Keep the ISO on your device. Do not upload the ISO to this public repository.
Install Python 3 in Termux if it is not already available, then:

    termux-setup-storage
    pkg install python
    python /path/to/accel3_probe.py "/sdcard/Download/your-Accel-3.iso" --out "/sdcard/Download/accel3-report.json"

On desktop:

    python3 accel3_probe.py "/path/to/your-Accel-3.iso" --out accel3-report.json

Only the small JSON report is needed for the next reverse-engineering step. Review it
before sharing; it contains filenames and offsets but should not contain game assets.

## Build/test

The GitHub Actions workflow build-accel3-probe.yml runs the standard-library tests
and publishes a source ZIP artifact. The script itself is portable Python and needs
no native compilation or third-party packages.

## References checked

- Game ID / dump metadata: https://renascene.com/psp/info/umd/2058
- Additional title identity and release record: https://titlescreen.org/release/r4z5fk43/naruto-shippuden-narutimate-accel-3
- PPSSPP texture replacement example for ULJS00236:
  https://vtmods.blogspot.com/2017/07/mod-texture-naruto-shippuden-ultimate.html
- UNIT: https://github.com/Raw-man/UNIT
- NNL: https://github.com/Raw-man/NNL
- Storm XFBIN 3ds Max importer (not confirmed for Accel 3):
  https://github.com/DennisHerrm/XFBIN-Import-3ds-max
