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
| 5 | Obito + Kaguya, support characters, 4 arenas, boss cinematics, Ch3-5 | Compiles, untested |
| 6 | Story expansion (multi-stage Ch6, more missions) | Planned |
| 7 | Optimization, real models, polish | Planned |

### What exists
- 8 fighters: Naruto, Sasuke, Hashirama, Madara, Kakashi, Sakura, Obito, Kaguya. Each has 3 distinct jutsu and an Ultimate; Naruto, Sasuke, Hashirama, Madara and Obito also have an Awakening.
- 13 move types (dash strike, clone barrage, projectile, spread, homing, ninken barrage, telegraphed ground strike, wide slash, area push, teleport strike, heal, burn, Kamui phase).
- Ultimates with cinematic camera and per-character effects.
- Support characters: L + Square calls a partner (homing strike) when the green support bar is full. Story missions assign partners (Team 7 rotates two).
- Boss phases defined per mission; each triggers a short transformation cinematic and powers the boss up.
- 4 arenas, free battle with random arena and support partners.
- Story Mode: chapter list, typewriter dialogue, unlock chain, save/load.

### Story missions (data in `missions[]` in `src/main.c`)
| Chapter | Mission | State |
|---|---|---|
| 1 | Hashirama vs Madara (3 phases) | Playable |
| 2 | Fourth Great Ninja War (Kakashi vs Madara) | Playable - placeholder pairing, not canon |
| 3 | Obito and the Ten-Tails (Naruto, support Sasuke/Kakashi) | Playable - simplified 1v1 |
| 4 | Madara Uchiha (Sasuke, support Naruto/Sakura) | Playable - simplified 1v1 |
| 5 | Kaguya Otsutsuki (Naruto, Team 7 support) | Playable - simplified 1v1 |
| 6 | Naruto vs Sasuke (Valley of the End) | Playable - single battle, not the full multi-phase fight |

Unlock order: Ch1 -> Ch2 -> Ch3 -> Ch4 -> Ch5 -> Ch6. All missions are simplified one-on-one battles; the original game's multi-stage scripted fights, QTEs and cutscenes are NOT implemented. Dialogue is original text, not the game's script.

## Controls
| Button | Action |
|---|---|
| Analog / D-pad | Move (relative to opponent) |
| Circle | Attack (tap to chain 4-hit combo) |
| Square | Jutsu 1; D-pad Up + Square = Jutsu 2; D-pad Down + Square = Jutsu 3 |
| L + Square | Support partner strike (needs full support bar) |
| Triangle (hold) | Charge chakra |
| Triangle + Cross | Chakra dash toward the locked-on opponent (15 chakra); a hit can start a combo |
| L + R together while in hit-stun | Substitution Jutsu (10 chakra; escapes hit-stun) |
| Triangle (hold) + Circle at full chakra | Ultimate Jutsu |
| L + R + Triangle when awakening bar is full | Awakening / transformation |
| Cross | Jump |
| L | Guard (combo finisher, big-stun jutsu and Ultimates break guard) |
| R | Dodge / substitution (10 chakra) |
| Start | Pause (Select while paused: quit) |

## Storm-style combat pass (prototype)
- Added a homing chakra dash with a dash-hit that can start a melee combo.
- Added a substitution escape during hit-stun, costing 10 chakra and granting brief invulnerability.
- R-trigger alone remains an evasive ninja move.
- This is inspired by the original game's combat rhythm, not an exact recreation; character animations, advanced combo routes, support switching and full original move timing still need work.

## Save data
Progress is written to `ms0:/PSP/SAVEDATA/NARUTOSTORM4/save.bin` (small file with checksum). If that path is not writable, the mission-complete screen says so. Story menu: Square twice erases progress.

Unlocks: Madara after Ch1, Kakashi after Ch2, Obito after Ch3, Kaguya after Ch5. Naruto, Sasuke, Hashirama, Sakura start unlocked.

## Build
Push to `main`; GitHub Actions builds `EBOOT.PBP` with the pspdev toolchain.
Download it from the Actions run artifacts, then place it in `PSP/GAME/NarutoStorm4/` for PPSSPP.

## Data files
`data/characters/*.json` and `data/missions/*.json` document the roster/story design but are **not loaded by the game yet**; the playable data is compiled into `src/main.c`.

## Known limitations
- Placeholder box models, no real animation, no audio, text-only dialogue.
- Roster is 8 of 100+ Storm 4 characters.
- Balance, AI difficulty and performance (30 FPS target) unverified on a device.
