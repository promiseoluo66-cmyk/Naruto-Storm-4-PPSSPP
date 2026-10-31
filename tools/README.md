# Model pipeline tools

## Static placeholder mesh
Blender / editor -> OBJ -> tools/obj2nmdl.py -> NMDL v2 -> src/model.c.

## Imported weighted character
Nested ZIP -> tools/import_smd_zip.py -> NRIG + NMTX -> src/rigged_model.c.

| Tool | What it does |
|---|---|
| make_test_character.py OUT_DIR | Generates the original low-poly test character and its test texture. |
| obj2nmdl.py in.obj out.nmdl | Converts a static OBJ mesh to NMDL v2. |
| import_obj_zip.py source.zip out_prefix | Imports the OBJ version of the Naruto mesh for the standalone viewer. |
| import_smd_zip.py source.zip out_prefix | Imports the Naruto SMD reference mesh, its 135-bone hierarchy, vertex weights and texture atlas into out_prefix.nrig and out_prefix.nmtx. |
| embed_assets.py out.c name=file ... | Embeds fallback binary assets in generated C source. |
| check_nmdl.py model.nmdl [tex.nmtx] [preview.png] | Validates static NMDL files and can create a software preview. |

The Makefile runs asset generation automatically. The SMD importer requires Python 3 and Pillow. GitHub Actions installs Pillow before building. A local build environment must also install Pillow before running make.

## NRIG format and animation

NRIG v1 stores a bind-pose vertex stream, up to three bone indices and weights per vertex, and each bone's parent index plus local bind matrix. The SMD supplied with the Naruto archive has a reference pose at time 0 but no animation clips. The PSP runtime computes the bone skin matrices and applies an original procedural walk cycle to the leg, knee, foot and arm bones. This animation responds to the fighter's analog movement blend.

The imported rig is currently used for Naruto's Free Battle roster entry only. Other fighters keep their existing prototype renderer. This is an initial runtime skeletal path, not a general-purpose animation clip player.

Generated files under assets/chr/test are ignored by Git and are recreated by the build workflow. The installer includes generated NRIG/NMTX files, but excludes the original source ZIP.

Only use models and textures that you made or have permission to use and redistribute. The third-party game extraction in the supplied archive is not cleared for public redistribution.
