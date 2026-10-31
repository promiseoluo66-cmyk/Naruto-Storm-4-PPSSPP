# Naruto SMD rig audit

Generated from the actual nested model archive by tools/audit_smd_rig.py.
Read-only extraction: no gameplay, importer, or animation source was changed.

## Source
- Outer ZIP SHA-256: 6b6eaeaf98c7c91e64460af910a8caf555be3d29a2ecef9c88c0164a025c3b0c
- Nested ZIP: source/Wii - Naruto Shippuden Clash of Ninja Revolution 3 - Naruto.zip
- Selected SMD: Naruto_3/Naruto.smd
- SMD SHA-256: 7d76b1cd6a7eae32be970af4b2782cf7d96da0ac1a8d5d35b8ceb7ce11e8afb2

## Verified counts
- Bones: 135; IDs 0 through 134.
- Root IDs: [0, 1, 2, 3, 4, 5].
- Skeleton frame times: [0].
- Triangles: 3537.
- SMD triangle-corner records: 10611.
- Distinct position tuples (diagnostic only): 1941.
- Distinct complete attribute tuples (diagnostic only): 2909.
- Original influence histogram: {1: 8711, 2: 1872, 3: 28}.
- Maximum original influences: 3.
- Records above runtime four-influence limit: 0.
- Parent links valid; all time-0 transforms present; all weight bone IDs valid: yes.
- Maximum global times inverse-bind identity residual: 8.8817842e-16.

## Runtime name comparison
- Names used by current C pose code: ['L_harm', 'L_hleg', 'L_larm', 'L_lleg', 'R_harm', 'R_hleg', 'R_larm', 'R_lleg', 'breast', 'head', 'waist'].
- Exact names matched in SMD: ['L_harm', 'L_hleg', 'L_larm', 'L_lleg', 'R_harm', 'R_hleg', 'R_larm', 'R_lleg', 'breast', 'head', 'waist'].
- Runtime names absent from SMD: [].
- Name truncation collisions: {}.
- Names longer than the importer's 31 UTF-8 byte limit: [].

## Implementation facts
- The C renderer calculates pose matrices in source coordinates and maps positions to (x,z,-y) after skinning.
- The SMD file provides reference pose samples; actual combat clips are not established by this file.
- The importer preserves source bone names up to 31 UTF-8 bytes and caps influences at four.
- Position tuple counts are diagnostic only; SMD triangle records are not a reusable indexed vertex pool.

## Output files
- skeleton_manifest.json: all bone names, IDs, parent/children, source transforms and calculated matrices.
- skeleton_hierarchy.txt: complete parent-child forest.
- skeleton_bones.csv: transforms and matrices for each bone.
- skin_weights.csv: every source triangle-corner record and all its source weights.
