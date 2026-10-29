#include <stdio.h>
#include <string.h>
#include "assets.h"

static int safe_relative_name(const char *name)
{
    const char *p;
    if (!name || !name[0] || name[0] == '/' || name[0] == '\\') return 0;
    for (p = name; *p; ++p) {
        if (*p == ':') return 0;
        if (*p == '.' && p[1] == '.') return 0;
        if (*p == '\\') return 0;
    }
    return 1;
}

int assets_build_path(char *out, unsigned int out_size,
                      AssetCategory category, const char *filename)
{
    const char *folder;
    int written;

    if (!out || out_size == 0 || !safe_relative_name(filename)) return -1;
    switch (category) {
    case ASSET_CHARACTER: folder = "assets/chr"; break;
    case ASSET_STAGE:     folder = "assets/stg"; break;
    case ASSET_UI:        folder = "assets/ui"; break;
    default: return -1;
    }

    written = snprintf(out, out_size, "%s/%s", folder, filename);
    if (written < 0 || (unsigned int)written >= out_size) {
        out[0] = '\0';
        return -2;
    }
    return 0;
}
