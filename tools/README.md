# Model pipeline tools

Original model data flows like this:

    Blender / any editor --> OBJ --> tools/obj2nmdl.py --> .nmdl --> game (src/model.c)

| Tool | What it does |
|---|---|
| `make_test_character.py OUT_DIR` | Generates the ORIGINAL low-poly test character (OBJ) and a 64x64 test texture (NMTX). No third-party assets. |
| `obj2nmdl.py in.obj out.nmdl` | Converts OBJ (positions, UVs, optional per-vertex colours; quads/n-gons triangulated) to NMDL v2. |
| `embed_assets.py out.c name=file ...` | Embeds binary files into a C source so they ship inside the EBOOT. |
| `check_nmdl.py model.nmdl [tex.nmtx] [preview.png]` | Validates files like the PSP loader does; optional software preview (not PSP output). |

The Makefile runs the first three automatically (it needs `python3`; Termux: `pkg install python`).
Generated files (`src/gen_assets.c`, `assets/chr/test/*`) are git-ignored.

## Using your own model
1. Export OBJ from Blender (triangulate, include UVs; vertex colours optional).
2. `python3 tools/obj2nmdl.py mychar.obj assets/chr/mychar/model.nmdl`
3. Load in C with `model_load(&m, "assets/chr/mychar/model.nmdl")` (or `model_load_memory` for embedded data).
4. Keep models under ~2000 triangles each on PSP; texture sizes power of two, 8-512.

Only use models you made or have the right to use. Do not put models extracted from commercial games in this project.

Skeleton/animation import is NOT implemented yet: NMDL is a static mesh.
