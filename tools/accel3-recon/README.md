# Naruto PSP Unified Asset Toolkit

One front-end routes between two separate game-specific workflows:

- Narutimate Accel 3 / Ultimate Ninja Heroes 3 (ULJS-00236 / ULUS-10518).
- Ultimate Ninja Impact (UNIT/NNL toolchain).

## Why adapters?

The games do not have a confirmed shared archive format. One launcher can detect
the game and route to the correct adapter, but the parsers must remain separate.
This avoids falsely treating Impact's format as compatible with Accel 3.

## Current verified scope

- Shared CLI front-end.
- PARAM.SFO-based title detection where metadata is present.
- Accel 3 adapter runs read-only ISO inventory.
- Impact adapter checks for UNIT and captures its help text.
- Common JSON reporting and automated tests.

Not yet implemented: proprietary archive decryption/unpacking, model/skeleton
export, animation clip export, or moveset/action-table export. The Impact adapter
does not assume an undocumented UNIT export command. Accel 3 still needs its
actual archive format identified from a report made from a real ISO.

## Usage

    python naruto_asset_tool.py /path/to/game.iso --game auto --out report.json

Override detection only if you have verified the game:

    python naruto_asset_tool.py /path/to/accel3.iso --game accel3
    python naruto_asset_tool.py /path/to/impact.iso --game impact --unit /path/to/unit

Keep ISO files and extracted copyrighted game assets local. Do not commit them.
Only share a reviewed metadata report, not the ISO.

## Files

- naruto_asset_tool.py: common launcher and adapter routing.
- accel3_probe.py: Accel 3 ISO inventory backend.
- test files: automated tests.
- Impact UNIT build setup remains on its existing isolated branch.

## Next stages

1. Validate detection against real PARAM.SFO files from both games.
2. Inspect UNIT's actual commands and validate the Impact extraction path.
3. Reverse-engineer Accel 3's proprietary archive/container format.
4. Implement each game's parser behind a common export schema.
5. Validate exported meshes, skeletons, animations and action metadata.
