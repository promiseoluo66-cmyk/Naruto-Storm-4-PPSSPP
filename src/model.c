#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include "model.h"
#include "assets.h"

#define NMDL_VERSION 2
#define NMTX_VERSION 1
#define NMDL_MAX_VERTICES 120000
#define NMTX_MAX_DIMENSION 512
#define NMTX_MIN_DIMENSION 8
#define NMTX_FORMAT_RGB565 1
#define NMTX_FORMAT_RGBA8888 3

typedef struct {
    char magic[4];
    unsigned int version;
    unsigned int vertex_count;
} NmdlHeader;

typedef struct {
    char magic[4];
    unsigned int version;
    unsigned int width;
    unsigned int height;
    unsigned int format;
} NmtxHeader;

static int read_exact(SceUID fd, void *dst, unsigned int bytes)
{
    return sceIoRead(fd, dst, bytes) == (int)bytes ? 0 : -1;
}

static int is_power_of_two(unsigned int value)
{
    return value >= NMTX_MIN_DIMENSION &&
           value <= NMTX_MAX_DIMENSION &&
           (value & (value - 1)) == 0;
}

int model_load(Model *model, const char *path)
{
    SceUID fd;
    NmdlHeader header;
    unsigned int i;

    if (!model || !path) return -1;
    model->vertices = NULL;
    model->vertex_count = 0;

    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return -2;
    if (read_exact(fd, &header, sizeof(header)) < 0) {
        sceIoClose(fd);
        return -3;
    }
    if (memcmp(header.magic, "NMDL", 4) != 0 ||
        (header.version != 1 && header.version != NMDL_VERSION) ||
        header.vertex_count == 0 ||
        header.vertex_count > NMDL_MAX_VERTICES ||
        (header.vertex_count % 3) != 0) {
        sceIoClose(fd);
        return -4;
    }

    model->vertices = (ModelVertex *)memalign(16, header.vertex_count * sizeof(ModelVertex));
    if (!model->vertices) {
        sceIoClose(fd);
        return -5;
    }

    for (i = 0; i < header.vertex_count; ++i) {
        ModelVertex *v = &model->vertices[i];
        if (header.version == 1) {
            unsigned int old_color;
            float old_x, old_y, old_z;
            if (read_exact(fd, &old_color, sizeof(old_color)) < 0 ||
                read_exact(fd, &old_x, sizeof(old_x)) < 0 ||
                read_exact(fd, &old_y, sizeof(old_y)) < 0 ||
                read_exact(fd, &old_z, sizeof(old_z)) < 0) {
                free(model->vertices);
                model->vertices = NULL;
                sceIoClose(fd);
                return -6;
            }
            v->u = 0.0f;
            v->v = 0.0f;
            v->color = old_color;
            v->x = old_x;
            v->y = old_y;
            v->z = old_z;
        } else {
            if (read_exact(fd, &v->u, sizeof(v->u)) < 0 ||
                read_exact(fd, &v->v, sizeof(v->v)) < 0 ||
                read_exact(fd, &v->color, sizeof(v->color)) < 0 ||
                read_exact(fd, &v->x, sizeof(v->x)) < 0 ||
                read_exact(fd, &v->y, sizeof(v->y)) < 0 ||
                read_exact(fd, &v->z, sizeof(v->z)) < 0) {
                free(model->vertices);
                model->vertices = NULL;
                sceIoClose(fd);
                return -6;
            }
        }
    }

    sceIoClose(fd);
    model->vertex_count = header.vertex_count;
    /* make the CPU-written vertices visible to the GE */
    sceKernelDcacheWritebackInvalidateRange(model->vertices,
                                            header.vertex_count * sizeof(ModelVertex));
    return 0;
}

/* Load an NMDL v2 mesh that is already in memory (e.g. embedded in the EBOOT). */
int model_load_memory(Model *model, const void *data, unsigned int size)
{
    NmdlHeader header;
    unsigned int bytes;

    if (!model || !data) return -1;
    model->vertices = NULL;
    model->vertex_count = 0;
    if (size < sizeof(header)) return -3;
    memcpy(&header, data, sizeof(header));
    if (memcmp(header.magic, "NMDL", 4) != 0 ||
        header.version != NMDL_VERSION ||
        header.vertex_count == 0 ||
        header.vertex_count > NMDL_MAX_VERTICES ||
        (header.vertex_count % 3) != 0) {
        return -4;
    }
    bytes = header.vertex_count * (unsigned int)sizeof(ModelVertex);
    if (size < sizeof(header) + bytes) return -6;

    model->vertices = (ModelVertex *)memalign(16, bytes);
    if (!model->vertices) return -5;
    memcpy(model->vertices, (const unsigned char *)data + sizeof(header), bytes);
    model->vertex_count = header.vertex_count;
    sceKernelDcacheWritebackInvalidateRange(model->vertices, bytes);
    return 0;
}

int model_load_asset(Model *model, AssetCategory category, const char *filename)
{
    char path[256];
    int result = assets_build_path(path, sizeof(path), category, filename);
    if (result < 0) return -7;
    return model_load(model, path);
}

int model_texture_load(ModelTexture *texture, const char *path)
{
    SceUID fd;
    NmtxHeader header;
    unsigned int bytes;

    if (!texture || !path) return -1;
    texture->pixels = NULL;
    texture->width = texture->height = texture->format = 0;

    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return -2;
    if (read_exact(fd, &header, sizeof(header)) < 0) {
        sceIoClose(fd);
        return -3;
    }
    if (memcmp(header.magic, "NMTX", 4) != 0 ||
        header.version != NMTX_VERSION ||
        (header.format != NMTX_FORMAT_RGB565 && header.format != NMTX_FORMAT_RGBA8888) ||
        !is_power_of_two(header.width) ||
        !is_power_of_two(header.height)) {
        sceIoClose(fd);
        return -4;
    }

    bytes = header.width * header.height *
            (header.format == NMTX_FORMAT_RGB565 ? 2u : 4u);
    texture->pixels = memalign(16, bytes);
    if (!texture->pixels) {
        sceIoClose(fd);
        return -5;
    }
    if (read_exact(fd, texture->pixels, bytes) < 0) {
        free(texture->pixels);
        texture->pixels = NULL;
        sceIoClose(fd);
        return -6;
    }
    sceIoClose(fd);

    texture->width = header.width;
    texture->height = header.height;
    texture->format = header.format;
    sceKernelDcacheWritebackInvalidateRange(texture->pixels, bytes);
    return 0;
}

/* Load an NMTX v1 texture that is already in memory. */
int model_texture_load_memory(ModelTexture *texture, const void *data, unsigned int size)
{
    NmtxHeader header;
    unsigned int bytes;

    if (!texture || !data) return -1;
    texture->pixels = NULL;
    texture->width = texture->height = texture->format = 0;
    if (size < sizeof(header)) return -3;
    memcpy(&header, data, sizeof(header));
    if (memcmp(header.magic, "NMTX", 4) != 0 ||
        header.version != NMTX_VERSION ||
        (header.format != NMTX_FORMAT_RGB565 && header.format != NMTX_FORMAT_RGBA8888) ||
        !is_power_of_two(header.width) ||
        !is_power_of_two(header.height)) {
        return -4;
    }
    bytes = header.width * header.height *
            (header.format == NMTX_FORMAT_RGB565 ? 2u : 4u);
    if (size < sizeof(header) + bytes) return -6;

    texture->pixels = memalign(16, bytes);
    if (!texture->pixels) return -5;
    memcpy(texture->pixels, (const unsigned char *)data + sizeof(header), bytes);
    texture->width = header.width;
    texture->height = header.height;
    texture->format = header.format;
    sceKernelDcacheWritebackInvalidateRange(texture->pixels, bytes);
    return 0;
}

int model_texture_load_asset(ModelTexture *texture, AssetCategory category,
                             const char *filename)
{
    char path[256];
    int result = assets_build_path(path, sizeof(path), category, filename);
    if (result < 0) return -7;
    return model_texture_load(texture, path);
}

void model_texture_bind(const ModelTexture *texture)
{
    if (!texture || !texture->pixels ||
        !is_power_of_two(texture->width) || !is_power_of_two(texture->height)) {
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }

    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(texture->format == NMTX_FORMAT_RGB565 ? GU_PSM_5650 : GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(0, texture->width, texture->height,
                  texture->width, texture->pixels);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
}

void model_texture_unbind(void)
{
    sceGuDisable(GU_TEXTURE_2D);
}

static void draw_internal(const Model *model, const ModelTexture *texture,
                          float x, float y, float z, float yaw, int textured)
{
    ScePspFVector3 position;
    unsigned int format = GU_TEXTURE_32BITF | GU_COLOR_8888 |
                          GU_VERTEX_32BITF | GU_TRANSFORM_3D;

    if (!model || !model->vertices || model->vertex_count == 0) return;
    if (textured && texture && texture->pixels) {
        model_texture_bind(texture);
    } else {
        model_texture_unbind();
    }

    position.x = x;
    position.y = y;
    position.z = z;
    sceGumMatrixMode(GU_MODEL);
    sceGumPushMatrix();
    sceGumTranslate(&position);
    if (yaw != 0.0f) sceGumRotateY(yaw);
    sceGumUpdateMatrix();
    sceGuDrawArray(GU_TRIANGLES, format, model->vertex_count, NULL, model->vertices);
    sceGumPopMatrix();

    model_texture_unbind();
}

void model_draw(const Model *model, float x, float y, float z, float yaw)
{
    draw_internal(model, NULL, x, y, z, yaw, 0);
}

void model_draw_textured(const Model *model, const ModelTexture *texture,
                         float x, float y, float z, float yaw)
{
    draw_internal(model, texture, x, y, z, yaw, 1);
}

void model_free(Model *model)
{
    if (!model) return;
    if (model->vertices) free(model->vertices);
    model->vertices = NULL;
    model->vertex_count = 0;
}

void model_texture_free(ModelTexture *texture)
{
    if (!texture) return;
    if (texture->pixels) free(texture->pixels);
    texture->pixels = NULL;
    texture->width = texture->height = texture->format = 0;
}
