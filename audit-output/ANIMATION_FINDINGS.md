# Animation findings from the extracted Naruto rig

This file documents source-backed findings only. It does not change gameplay code.

## Confirmed hierarchy and name mapping

- The selected asset is Naruto_3/Naruto.smd.
- The actual main hierarchy starts at node 5, nw4r_root, then Y -> X -> Z -> hip.
- Nodes 0-4 are named NotABone0 through NotABone4 and have no parent.
- The exact bone-name strings used by src/skinned.c all exist in the source SMD:
  L_harm, R_harm, L_larm, R_larm, L_hleg, R_hleg, L_lleg, R_lleg, breast, waist, head.
- Therefore a missing Bip01 prefix or a complete name-lookup failure is NOT the cause for these 11 hooks.

## Why current movement is likely unnatural

1. Only 11 of 135 nodes receive procedural rotations. Collar bones, hands, fingers, neck, feet, toes, hair/accessory nodes and most face nodes remain at reference pose.
2. The current code rotates upper and lower arm bones around local axis Z on both sides. The source bind matrices show that local Z is not the same character-space axis on left and right:
   - L_harm: [0, -0.7262, -0.6875]
   - R_harm: [0,  0.9876, -0.1569]
   - L_larm: [0, -0.7262, -0.6875]
   - R_larm: [0,  0.6946, -0.7194]
   Sign-mirroring the same local-axis angle therefore does not produce mirrored character-space motion. This is a strong candidate for asymmetric arm swing and elbow behaviour.
3. The jump action uses fixed upper-arm, forearm, upper-leg and lower-leg angles. It does not depend on jump timer, vertical velocity, ascent/descent phase or landing. The same pose is held throughout the airborne state.
4. The run phase is advanced by up to 0.35 radians per game frame, then pose_bone multiplies it by 2.6 before taking sin(). At maximum input that is about 0.91 radians per frame, a full cycle every 6.9 frames. At a 30 FPS target this is about 4.35 cycles per second.
5. The skinned renderer rebuilds every pose from the bind pose each draw and applies the current action directly. There is no cross-fade/blend between idle, run, jump and attack poses.
6. src/storm_anim.c is not the animation path used for the imported Naruto mesh: the skeletal branch in the patched draw function calls skinned_draw() and returns before the legacy procedural fighter drawing path.

## What is not the problem

- All 11 current runtime name lookups match the source SMD exactly.
- The 135-node parent links and the time-0 reference transforms validate.
- The source has no records with more than four influences; the runtime's four-influence cap discards no weights from this asset.
- The global/inverse-bind matrix identity check residual is approximately 8.9e-16 using the same matrix convention as the importer/runtime.

## Facial rig

The source includes head node 86, a face-related branch at 97-101 (ago, mouth_UNDER, mouth_L, mouth_R, mouth_TOP), hair/accessory branches and separate model attachment nodes. It has no separately named nose or eye bone. Do not invent nose/eye joints; inspect the mesh objects/textures to establish how those details are represented.

## Next engineering step

Before writing complete animation tracks, calibrate shoulder/elbow and hip/knee rotations one joint at a time against this bind pose. Convert desired character-space rotations into the correct per-bone local axes. Then add pose blending and time-dependent jump/landing phases. Test each stage in PPSSPP before adding combat combos.