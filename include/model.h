#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

/*
 * NMDL v1 model format:
 *   4 bytes  magic "NMDL"
 *   u32      version (1)
 *   u32      vertex_count (non-zero multiple of 3)
 *   vertex_count records of:
 *     u32 color in PSP GU RGBA8888 layout
 *     f32 x, y, z
 *
 * All numeric fields are little-endian, matching the PSP's native format.
 * This first version supports colored triangle meshes. Textures, UVs,
 * normals and skeletal animation are later pipeline milestones.
 */
typedef struct {
    unsigned int color;
    float x, y, z;
} ModelVertex;

typedef struct {
    ModelVertex *vertices;
    unsigned int vertex_count;
} Model;

int model_load(Model *model, const char *path);
void model_draw(const Model *model, float x, float y, float z, float yaw);
void model_free(Model *model);

#endif
