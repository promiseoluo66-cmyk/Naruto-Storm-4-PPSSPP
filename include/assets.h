#ifndef ASSETS_H
#define ASSETS_H

/* Runtime asset categories relative to the launched PSP game directory. */
typedef enum {
    ASSET_CHARACTER = 0,
    ASSET_STAGE = 1,
    ASSET_UI = 2
} AssetCategory;

/*
 * Build a path such as "assets/chr/naruto.nmdl".
 * Returns 0 on success, -1 for invalid input, or -2 if output is too small.
 * The caller supplies storage; no allocation is performed.
 */
int assets_build_path(char *out, unsigned int out_size,
                      AssetCategory category, const char *filename);

#endif
