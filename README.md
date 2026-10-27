# Naruto Storm 4 - PPSSPP Edition

A Naruto Ultimate Ninja Storm 4-inspired fighting game for PSP, built to run in PPSSPP.
Unofficial fan project. Uses only original placeholder models; no proprietary assets.

## Status (honest)

| Phase | State |
|---|---|
| 1 - PSP foundation, build workflow | Compiles in CI. Not yet confirmed booting in PPSSPP. |
| 2 - Basic 3D combat | Code written, build pending verification. Untested in PPSSPP. |
| 3-7 | Planned |

### Phase 2 contents
- Real 3D GU rendering (placeholder box models), lock-on camera
- Movement, jump, 4-hit combo, guard (finisher breaks guard), dodge (R), chakra charge
- Ninjutsu projectile per character (cost 25 chakra)
- AI opponent, health + chakra bars, KO / victory / defeat, retry
- Free Battle with 6 selectable fighters; Story Ch1 mission (Hashirama vs Madara, 3 boss phases)

## Controls
| Button | Action |
|---|---|
| Analog / D-pad | Move (relative to opponent) |
| Circle | Attack (tap to chain combo) |
| Square | Ninjutsu (25 chakra) |
| Cross | Jump |
| L | Guard |
| R | Dodge / substitution (10 chakra) |
| Triangle (hold) | Charge chakra |
| Start | Pause |

## Build
Push to `main`; GitHub Actions builds `EBOOT.PBP` with the pspdev toolchain.
Download it from the Actions run artifacts, then place it in `PSP/GAME/NarutoStorm4/` for PPSSPP.

## Data
`data/characters/*.json` and `data/missions/*.json` define the roster and story.
They are not loaded by the engine yet (planned for Phase 3-4).
