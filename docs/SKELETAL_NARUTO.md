# Naruto skeletal model pipeline

## What was found in the asset archive

The nested archive contains Naruto.smd and Naruto_9T.smd. Both contain a 135-node hierarchy and one skeleton sample at time 0; they are reference poses, not animation clips. The regular Naruto SMD is used for the Free Battle player and training dummy.

The SMD reference mesh contains 3,537 triangles / 10,611 triangle vertices. Its vertex influence distribution is:

| Influences per vertex | Vertices |
|---:|---:|
| 1 | 8,711 |
| 2 | 1,872 |
| 3 | 28 |

This is a real weighted rig: vertices reference named bones and the SMD contains a parent hierarchy. It is not just a static OBJ with a bone list.

## Build conversion

tools/import_smd_zip.py reads the nested archive and converts the SMD into:

- assets/chr/test/naruto_skin.nskl — custom NSKM v1 skeleton + weighted mesh.
- assets/chr/test/naruto_skin.nmtx — 512×512 RGBA texture atlas in the existing NMTX format.

The importer validates the hierarchy, reference pose, triangle records, bone IDs and weights. It keeps up to four influences per vertex and normalizes their weights. The current Naruto source uses at most three.

tools/import_obj_zip.py still generates the older static NMDL viewer mesh. The battle renderer uses the new weighted SMD mesh instead.

## Runtime

include/skinned.h and src/skinned.c provide:

- SMD bind-pose hierarchy loading.
- Parent-to-child bind matrix calculation and inverse-bind matrices.
- CPU weighted linear-blend skinning.
- Separate dynamic vertex buffers for the player and stationary training dummy.
- Texture atlas rendering through the existing PSP GU model renderer.
- SMD Z-up to game Y-up coordinate conversion.

The training scene maps existing fighter states to procedural bone poses for idle, run, attack, jutsu, jump, guard and dash. These are newly authored procedural rotations on the imported rig, not original Storm 4 animation clips.

tools/patch_skeletal_battle.py applies the Free Battle rendering hook to src/main.c during the Makefile build. It is idempotent so make clean followed by make can run the generation step more than once.

## Known limitations and test status

- No motion clips are included in the source SMDs; exact Naruto game animations are not available in this archive.
- The procedural pose axes, model scale, facing direction, atlas appearance and frame rate still require visual testing in PPSSPP.
- If the skeletal asset fails to load, the existing procedural fighter renderer remains as a fallback.
- GitHub Actions verifies conversion, compilation and packaging. It does not verify the visual result inside PPSSPP.