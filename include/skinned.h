#ifndef SKINNED_H
#define SKINNED_H

#include "model.h"

#define SKIN_MAX_BONES 256

typedef struct {
    int parent;
    char name[32];
    float local_bind[12];
    float bind_global[12];
    float inverse_bind[12];
} SkinBone;

typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    unsigned short bone[4];
    float weight[4];
} SkinVertex;

typedef struct {
    unsigned int bone_count;
    unsigned int vertex_count;
    SkinBone *bones;
    SkinVertex *vertices;
    ModelTexture texture;
} SkinnedMesh;

typedef enum {
    SKIN_ACTION_IDLE = 0,
    SKIN_ACTION_RUN,
    SKIN_ACTION_ATTACK,
    SKIN_ACTION_JUTSU,
    SKIN_ACTION_JUMP,
    SKIN_ACTION_GUARD,
    SKIN_ACTION_DASH,
    SKIN_ACTION_HIT
} SkinAction;

typedef struct {
    SkinAction action;
    int combo;
    int timer;
    int combo_dir;
    float phase;
    float height;
    float vertical_velocity;
} SkinPose;

typedef struct {
    ModelVertex *vertices;
    float global[SKIN_MAX_BONES][12];
    float skin[SKIN_MAX_BONES][12];
    SkinPose last_pose;
    SkinPose transition_pose;
    int pose_valid;
    int blend_frames_left;
} SkinnedInstance;

int skinned_mesh_load(SkinnedMesh *mesh, const char *mesh_path,
                      const char *texture_path);
int skinned_instance_init(SkinnedInstance *instance, const SkinnedMesh *mesh);
void skinned_instance_free(SkinnedInstance *instance);
void skinned_mesh_free(SkinnedMesh *mesh);
void skinned_draw(const SkinnedMesh *mesh, SkinnedInstance *instance,
                  float x, float y, float z, float yaw, float scale,
                  const SkinPose *pose);

#endif
