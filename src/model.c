#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"

#define NMDL_VERSION 1
#define NMDL_MAX_VERTICES 120000

typedef struct {
    char magic[4];
    unsigned int version;
    unsigned int vertex_count;
} NmdlHeader;

int model_load(Model *model, const char *path)
{
    SceUID fd;
    NmdlHeader header;
    unsigned int bytes;
    if (!model || !path) return -1;
    model->vertices = NULL;
    model->vertex_count = 0;

    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return -2;
    if (sceIoRead(fd, &header, sizeof(header)) != sizeof(header)) {
        sceIoClose(fd);
        return -3;
    }
    if (memcmp(header.magic, "NMDL", 4) != 0 ||
        header.version != NMDL_VERSION ||
        header.vertex_count == 0 ||
        header.vertex_count > NMDL_MAX_VERTICES ||
        (header.vertex_count % 3) != 0) {
        sceIoClose(fd);
        return -4;
    }

    bytes = header.vertex_count * (unsigned int)sizeof(ModelVertex);
    model->vertices = (ModelVertex *)malloc(bytes);
    if (!model->vertices) {
        sceIoClose(fd);
        return -5;
    }
    if (sceIoRead(fd, model->vertices, bytes) != (int)bytes) {
        free(model->vertices);
        model->vertices = NULL;
        sceIoClose(fd);
        return -6;
    }
    sceIoClose(fd);
    model->vertex_count = header.vertex_count;
    return 0;
}

void model_draw(const Model *model, float x, float y, float z, float yaw)
{
    ScePspFVector3 position;
    if (!model || !model->vertices || model->vertex_count == 0) return;
    position.x = x;
    position.y = y;
    position.z = z;

    sceGumMatrixMode(GU_MODEL);
    sceGumPushMatrix();
    sceGumTranslate(&position);
    if (yaw != 0.0f) sceGumRotateY(yaw);
    sceGumUpdateMatrix();
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        model->vertex_count, NULL, model->vertices);
    sceGumPopMatrix();
}

void model_free(Model *model)
{
    if (!model) return;
    if (model->vertices) free(model->vertices);
    model->vertices = NULL;
    model->vertex_count = 0;
}
