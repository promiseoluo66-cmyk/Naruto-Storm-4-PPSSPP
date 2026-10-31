#ifndef RIGGED_MODEL_H
#define RIGGED_MODEL_H

#include "model.h"

/* Lightweight CPU skinning for the PSP's imported Naruto SMD rig. */
typedef struct {
    int parent;
    float bind_local[12];
    float bind_global[12];
    float inverse_bind[12];
    float current_local[12];
    float current_global[12];
    float skin[12];
} RiggedBone;

typedef struct {
    unsigned short bone[3];
    float weight[3];
} RiggedInfluence;

typedef struct {
    ModelVertex *bind_vertices;
    ModelVertex *skinned_vertices;
    RiggedInfluence *influences;
    RiggedBone *bones;
    unsigned int vertex_count;
    unsigned int bone_count;
    int loaded;
} RiggedModel;

int rigged_model_load(RiggedModel *model, const char *path);
void rigged_model_draw(RiggedModel *model, const ModelTexture *texture,
                       float x, float y, float z, float yaw,
                       float walk_phase, float move_blend,
                       int attack_combo, int attack_timer, float scale);
void rigged_model_free(RiggedModel *model);

#endif
