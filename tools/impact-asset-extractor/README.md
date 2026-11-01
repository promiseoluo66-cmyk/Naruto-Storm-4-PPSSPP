# Naruto Impact asset extraction toolchain

This folder documents the extraction toolchain being prepared for the PSP project.

## Correct tools

- **UNIT**: https://github.com/Raw-man/UNIT
  - Command-line toolbox for Naruto Shippuden: Ultimate Ninja Impact and Naruto Shippuden: Legends: Akatsuki Rising.
  - Handles game archive operations and several game-native asset conversions.
- **NNL**: https://github.com/Raw-man/NNL
  - The C++ library used by UNIT.
  - Its documented API can parse model, texture, animation, visibility-animation, and action-configuration containers and convert a character asset to GLB.
  - The NNL README example uses the animation container and action configuration to associate animation names.

## Important scope limits

1. The current UNIT CLI README lists model/environment, texture, text, minimap, and audio operations. Do not assume the CLI already exposes a complete animation/moveset export command.
2. NNL's C++ API documents animation and action-configuration parsing. The next engineering step is a small NNL-based extractor that exports those structures and produces an animation/action index. We must compile and test that against the actual Impact archive before claiming success.
3. NNL's README specifically notes that **NSUNI / Ultimate Ninja Impact archives require a decryption preparation step** before its documented archive parser can read them. That stage still needs to be confirmed for the user's USA dump.
4. NNL and UNIT document support for Ultimate Ninja Impact and Legends: Akatsuki Rising. They do **not** document support for the separate Japanese PSP game *Narutimate Accel 3 / Ultimate Ninja Heroes 3*.
5. No complete ISO, game archive, model, texture, animation, or other copyrighted game asset is stored in this repository or in the build artifact.

## GitHub Actions build

Workflow: `.github/workflows/build-impact-extractor.yml`

It builds UNIT from a pinned upstream commit on Ubuntu 24.04, verifies its CLI help, and uploads the Linux executable plus help/metadata as a 14-day Actions artifact. The artifact is a Linux x86-64 executable for later GitHub-runner use; it is not an Android app.

## References

- UNIT upstream: https://github.com/Raw-man/UNIT
- NNL upstream: https://github.com/Raw-man/NNL
- NNL README's documented animation extraction API: https://github.com/Raw-man/NNL#usage
