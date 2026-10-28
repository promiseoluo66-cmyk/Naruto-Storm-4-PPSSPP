# Naruto Storm 4 - PPSSPP Edition

A Naruto Ultimate Ninja Storm 4-inspired fighting game for PSP, built to run in PPSSPP.
Unofficial fan project. Uses only original placeholder models; no proprietary assets.

## Status (honest)

"Compiles" means the GitHub Actions build produced an EBOOT.PBP. **Nothing has been run in PPSSPP by the developer/AI yet**, so gameplay, balance, camera, frame rate and save writing are all untested.

| Phase | Content | State |
|---|---|---|
| 1 | PSP foundation, build workflow | Compiles |
| 2 | 3D arena combat, lock-on camera, AI | Compiles, untested |
| 3 | Character jutsu, Ultimates, awakening | Compiles, untested |
| 4 | Story Mode framework, dialogue, unlocks, save | Compiles, untested |
| 5-7 | More characters/arenas, support characters, story expansion, optimization | Planned |

### What exists
- 6 fighters: Naruto, Sasuke, Hashirama, Madara, Kakashi, Sakura. Each has 3 distinct jutsu, an Ultimate and (4 of them) an Awakening.
- 12 move types: dash strike, clone barrage, projectile, spread shot, homing, ninken barrage, telegraphed ground strike, wide slash, area push, teleport strike, heal, burn (Amaterasu).
- Ultimates play a cinematic camera + visual effect. Awakening boosts damage/speed and reduces damage taken for 30 s.
- Story Mode: chapter list, typewriter dialogue before/after battles, boss phases defined per mission, unlock chain, save/load.
- Free Battle with random arena (3 arenas), Character collection with lock status.

### Story missions (data in `missions[]` in `src/main.c`)
| Chapter | Mission | State |
|---|---|---|
| 1 | Hashirama vs Madara (3 phases) | Playable |
| 2 | Fourth Great Ninja War (Kakashi vs Madara) | Playable - placeholder pairing, not canon |
| 3 | Obito and the Ten-Tails | Not built (needs Obito) |
| 4 | Madara Uchiha | Not built |
| 5 | Kaguya Otsutsuki | Not built (needs Kaguya, Team 7) |
| 6 | Naruto vs Sasuke (Valley of the End) | Playable - single battle, not the full multi-phase fight |

Chapters 3-5 and the multi-stage story are not implemented yet. Cutscenes are text-over-3D-scene only.

## Controls
| Button | Action |
|---|---|
| Analog / D-pad | Move (relative to opponent) |
| Circle | Attack (tap to chain 4-hit combo) |
| Square | Jutsu 1; D-pad Up + Square = Jutsu 2; D-pad Down + Square = Jutsu 3 |
| Triangle (hold) | Charge chakra |
| Triangle (hold) + Circle at full chakra | Ultimate Jutsu |
| L + R + Triangle when awakening bar is full | Awakening / transformation |
| Cross | Jump |
| L | Guard (combo finisher, jutsu with big stun and Ultimates break guard) |
| R | Dodge / substitution (10 chakra) |
| Start | Pause (Select while paused: quit) |

## Save data
Progress is written to `ms0:/PSP/SAVEDATA/NARUTOSTORM4/save.bin` (a small file with a checksum). If PPSSPP's memory stick path is not writable, the mission-complete screen says so. Story menu: Square twice erases progress.

Unlocks: Madara after Ch1, Kakashi after Ch2. Naruto, Sasuke, Hashirama, Sakura start unlocked.

## Build
Push to `main`; GitHub Actions builds `EBOOT.PBP` with the pspdev toolchain.
Download it from the Actions run artifacts, then place it in `PSP/GAME/NarutoStorm4/` for PPSSPP.

## Data files
`data/characters/*.json` and `data/missions/*.json` document the roster/story design but are **not loaded by the game yet**; the playable data is compiled into `src/main.c`.
