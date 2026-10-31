# Naruto Storm 4 - PPSSPP Homebrew Prototype

A Naruto-inspired PSP fighting-game prototype built with the PSP SDK and tested through GitHub Actions. It is an unofficial fan project, not an official Naruto or Storm 4 release.

## Current build focus: Free Battle only

The current launcher enters the game menu instead of the standalone model viewer. The menu exposes **Free Battle** and **Exit** only. Story Mode and the character-database option are temporarily disabled in the interface; their old source code is retained on the feature branch so it can be restored later.

Free Battle flow:
1. Choose **Free Battle**.
2. Select a fighter from the roster.
3. Start a battle against an AI opponent.
4. Use the analog stick or D-pad to move and the PSP buttons to fight.

## Imported Naruto model

Naruto's roster entry now uses the imported weighted SMD model at runtime when its generated assets load successfully.

The source archive contains:
- A Naruto mesh with 3,537 triangles.
- A 135-bone skeleton hierarchy.
- Per-vertex skin weights (one to three influences per vertex).
- A reference pose only (time 0); it does **not** include a ready-made run animation clip.

The build creates an NRIG file containing the mesh, skin weights and bind-pose bone matrices, plus a 512×512 texture atlas. The PSP runtime CPU-skins the imported mesh and generates an original procedural movement cycle: opposite leg swings, knee bends and arm swings follow the player's analog movement blend. This is not the original game's animation data or a full animation-clip system.

Only Naruto currently uses this imported rig. Other roster entries still use the prototype procedural models.

## Controls

| Button | Action |
|---|---|
| Analog / D-pad | Move relative to the opponent |
| Circle | Attack; repeated taps can chain a combo |
| Square | Jutsu 1 |
| D-pad Up + Square | Jutsu 2 |
| D-pad Down + Square | Jutsu 3 |
| L + Square | Support partner strike when the support bar is full |
| L + Circle | Team Ultimate when requirements are met |
| Triangle (hold) | Charge chakra |
| Triangle + Cross | Chakra dash |
| Cross | Jump |
| L | Guard |
| R | Dodge |
| Start | Pause |

Touch controls are supplied by PPSSPP. The game reads PSP analog input; use the left analog control on the PPSSPP overlay to move.

## Build and install

GitHub Actions builds EBOOT.PBP using the PSP development toolchain. The workflow also creates NarutoStorm4-PSP.zip, containing the installable PSP/GAME/NarutoStorm4/ directory and external assets.

To install in PPSSPP:
1. Download the NarutoStorm4-PSP artifact from the successful GitHub Actions run.
2. Extract the ZIP.
3. Copy its PSP folder into the PPSSPP memory-stick directory.
4. Launch Naruto Storm 4 from PPSSPP.

The separate EBOOT.PBP artifact does not contain the external rig and texture files by itself; use the full NarutoStorm4-PSP.zip package for this build.

## Validation status

A successful GitHub Actions run confirms compilation and packaging only. It does not prove the game starts correctly, that the imported rig looks correct, that the analog animation is visually sound, or that performance is acceptable on a real PSP/PPSSPP device. These still require on-device testing.

## Known limitations

- Story Mode is disabled in the current interface.
- Only Naruto uses the imported weighted model; all other fighters remain prototype models.
- The imported SMD has a bind pose but no run/walk clip; the current gait is procedurally generated.
- CPU skinning performance, model orientation, texture appearance, camera framing, combat balance and save behaviour need testing in PPSSPP.
- The roster and move set are prototype systems, not a complete or exact Storm 4 recreation.
- Use only assets you own or are permitted to redistribute. The source model archive is a third-party game extraction and is not cleared for public redistribution.
