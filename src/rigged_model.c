#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <pspgum.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rigged_model.h"

#define NRIG_VERSION 1
#define NRIG_MAX_VERTICES 120000
#define NRIG_MAX_BONES 160

typedef struct {
    char magic[4];
    unsigned int version;
    unsigned int vertex_count;
    unsigned int bone_count;
} NrigHeader;

static int read_exact(SceUID fd, void *dst, unsigned int bytes)
{
    return sceIoRead(fd, dst, bytes) == (int)bytes ? 0 : -1;
}

/* Row-major affine 3x4 matrices. The importer has already converted axes. */

static void mat_copy(float dst[12], const float src[12])
{
    memcpy(dst, src, sizeof(float) * 12);
}

static void mat_mul(float out[12], const float a[12], const float b[12])
{
    float t[12];
    int r;
    for (r = 0; r < 3; r++) {
        int q = r * 4;
        t[q + 0] = a[q + 0] * b[0] + a[q + 1] * b[4] + a[q + 2] * b[8];
        t[q + 1] = a[q + 0] * b[1] + a[q + 1] * b[5] + a[q + 2] * b[9];
        t[q + 2] = a[q + 0] * b[2] + a[q + 1] * b[6] + a[q + 2] * b[10];
        t[q + 3] = a[q + 0] * b[3] + a[q + 1] * b[7] +
                   a[q + 2] * b[11] + a[q + 3];
    }
    mat_copy(out, t);
}

static void mat_inverse_rigid(float out[12], const float in[12])
{
    int r;
    /* The bind transforms are rotations + translations, so R inverse = R^T. */
    for (r = 0; r < 3; r++) {
        out[r * 4 + 0] = in[0 * 4 + r];
        out[r * 4 + 1] = in[1 * 4 + r];
        out[r * 4 + 2] = in[2 * 4 + r];
    }
    for (r = 0; r < 3; r++) {
        out[r * 4 + 3] = -(out[r * 4 + 0] * in[3] +
                           out[r * 4 + 1] * in[7] +
                           out[r * 4 + 2] * in[11]);
    }
}

static void mat_rotate_local_y(float m[12], float angle)
{
    float c = cosf(angle), s = sinf(angle);
    int r;
    /* Post-multiply by a local-Y rotation, leaving the joint origin fixed. */
    for (r = 0; r < 3; r++) {
        int q = r * 4;
        float x = m[q + 0], z = m[q + 2];
        m[q + 0] = x * c - z * s;
        m[q + 2] = x * s + z * c;
    }
}

static void setup_walk_pose(RiggedModel *model, float phase, float blend,
                            int attack_combo, int attack_timer)
{
    float swing, left_knee, right_knee, punch = 0.0f;
    unsigned int i;

    if (blend < 0.0f) blend = 0.0f;
    if (blend > 1.0f) blend = 1.0f;
    swing = sinf(phase) * 0.72f * blend;
    if (attack_combo > 0 && attack_timer > 0 && attack_timer <= 9) {
        if (attack_timer <= 2) punch = -0.18f * (float)attack_timer / 2.0f;
        else if (attack_timer <= 5) punch = -0.18f + 0.46f * (float)(attack_timer - 2);
        else punch = 1.20f * (1.0f - (float)(attack_timer - 5) / 4.0f);
    }
    left_knee = fmaxf(0.0f, sinf(phase + 0.65f)) * 0.82f * blend;
    right_knee = fmaxf(0.0f, sinf(phase + 3.14159265f + 0.65f)) * 0.82f * blend;

    for (i = 0; i < model->bone_count; i++)
        mat_copy(model->bones[i].current_local, model->bones[i].bind_local);

    /* Naruto SMD hierarchy: hip 9; legs 10-12 and 18-20; arms 44-46/65-67. */
    if (model->bone_count > 66) {
        mat_rotate_local_y(model->bones[10].current_local, swing);
        mat_rotate_local_y(model->bones[18].current_local, -swing);
        mat_rotate_local_y(model->bones[11].current_local, left_knee);
        mat_rotate_local_y(model->bones[19].current_local, right_knee);
        mat_rotate_local_y(model->bones[12].current_local, -left_knee * 0.22f);
        mat_rotate_local_y(model->bones[20].current_local, -right_knee * 0.22f);
        if (attack_combo > 0 && punch != 0.0f) {
            /* A basic strike alternates hands. The punch reaches full extension
               on frame 5, exactly when melee_hit() checks the hit box. */
            if ((attack_combo & 1) != 0) {
                mat_rotate_local_y(model->bones[65].current_local, punch);
                mat_rotate_local_y(model->bones[66].current_local, -punch * 0.12f);
                mat_rotate_local_y(model->bones[67].current_local, punch * 0.08f);
                mat_rotate_local_y(model->bones[44].current_local, -punch * 0.12f);
            } else {
                mat_rotate_local_y(model->bones[44].current_local, -punch);
                mat_rotate_local_y(model->bones[45].current_local, punch * 0.12f);
                mat_rotate_local_y(model->bones[46].current_local, -punch * 0.08f);
                mat_rotate_local_y(model->bones[65].current_local, punch * 0.12f);
            }
            mat_rotate_local_y(model->bones[41].current_local,
                               ((attack_combo & 1) ? 1.0f : -1.0f) * punch * 0.08f);
        } else {
            mat_rotate_local_y(model->bones[44].current_local, -swing * 0.48f);
            mat_rotate_local_y(model->bones[65].current_local, swing * 0.48f);
            mat_rotate_local_y(model->bones[45].current_local, fmaxf(0.0f, swing) * 0.16f);
            mat_rotate_local_y(model->bones[66].current_local, fmaxf(0.0f, -swing) * 0.16f);
            mat_rotate_local_y(model->bones[41].current_local, swing * 0.035f);
        }
    }

    for (i = 0; i < model->bone_count; i++) {
        RiggedBone *b = &model->bones[i];
        if (b->parent >= 0)
            mat_mul(b->current_global, model->bones[b->parent].current_global,
                    b->current_local);
        else
            mat_copy(b->current_global, b->current_local);
        mat_mul(b->skin, b->current_global, b->inverse_bind);
    }
}

static void skin_vertices(RiggedModel *model)
{
    unsigned int i;
    for (i = 0; i < model->vertex_count; i++) {
        const ModelVertex *src = &model->bind_vertices[i];
        ModelVertex *dst = &model->skinned_vertices[i];
        const RiggedInfluence *inf = &model->influences[i];
        float x = 0.0f, y = 0.0f, z = 0.0f;
        int k;

        dst->u = src->u;
        dst->v = src->v;
        dst->color = src->color;
        for (k = 0; k < 3; k++) {
            float w = inf->weight[k];
            unsigned int bi = inf->bone[k];
            const float *m;
            if (w <= 0.00001f || bi >= model->bone_count) continue;
            m = model->bones[bi].skin;
            x += (m[0] * src->x + m[1] * src->y + m[2] * src->z + m[3]) * w;
            y += (m[4] * src->x + m[5] * src->y + m[6] * src->z + m[7]) * w;
            z += (m[8] * src->x + m[9] * src->y + m[10] * src->z + m[11]) * w;
        }
        dst->x = x;
        dst->y = y;
        dst->z = z;
    }
    sceKernelDcacheWritebackInvalidateRange(model->skinned_vertices,
        model->vertex_count * sizeof(ModelVertex));
}

int rigged_model_load(RiggedModel *model, const char *path)
{
    SceUID fd;
    NrigHeader h;
    unsigned int i;
    if (!model || !path) return -1;
    memset(model, 0, sizeof(*model));

    fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return -2;
    if (read_exact(fd, &h, sizeof(h)) < 0 ||
        memcmp(h.magic, "NRIG", 4) != 0 ||
        h.version != NRIG_VERSION ||
        h.vertex_count == 0 || h.vertex_count > NRIG_MAX_VERTICES ||
        h.vertex_count % 3 != 0 ||
        h.bone_count == 0 || h.bone_count > NRIG_MAX_BONES) {
        sceIoClose(fd);
        return -3;
    }

    model->vertex_count = h.vertex_count;
    model->bone_count = h.bone_count;
    model->bind_vertices = (ModelVertex *)memalign(16, h.vertex_count * sizeof(ModelVertex));
    model->skinned_vertices = (ModelVertex *)memalign(16, h.vertex_count * sizeof(ModelVertex));
    model->influences = (RiggedInfluence *)memalign(16, h.vertex_count * sizeof(RiggedInfluence));
    model->bones = (RiggedBone *)memalign(16, h.bone_count * sizeof(RiggedBone));
    if (!model->bind_vertices || !model->skinned_vertices ||
        !model->influences || !model->bones) {
        sceIoClose(fd);
        rigged_model_free(model);
        return -4;
    }
    memset(model->bones, 0, h.bone_count * sizeof(RiggedBone));

    for (i = 0; i < h.vertex_count; i++) {
        if (read_exact(fd, &model->bind_vertices[i], sizeof(ModelVertex)) < 0 ||
            read_exact(fd, model->influences[i].bone, sizeof(unsigned short) * 3) < 0 ||
            read_exact(fd, model->influences[i].weight, sizeof(float) * 3) < 0) {
            sceIoClose(fd);
            rigged_model_free(model);
            return -5;
        }
    }
    for (i = 0; i < h.bone_count; i++) {
        RiggedBone *b = &model->bones[i];
        if (read_exact(fd, &b->parent, sizeof(int)) < 0 ||
            read_exact(fd, b->bind_local, sizeof(float) * 12) < 0) {
            sceIoClose(fd);
            rigged_model_free(model);
            return -6;
        }
        if (b->parent >= (int)i || b->parent < -1) {
            sceIoClose(fd);
            rigged_model_free(model);
            return -7;
        }
        if (b->parent >= 0)
            mat_mul(b->bind_global, model->bones[b->parent].bind_global, b->bind_local);
        else
            mat_copy(b->bind_global, b->bind_local);
        mat_inverse_rigid(b->inverse_bind, b->bind_global);
    }
    sceIoClose(fd);
    model->loaded = 1;
    setup_walk_pose(model, 0.0f, 0.0f, 0, 0);
    skin_vertices(model);
    return 0;
}

void rigged_model_draw(RiggedModel *model, const ModelTexture *texture,
                       float x, float y, float z, float yaw,
                       float walk_phase, float move_blend,
                       int attack_combo, int attack_timer, float scale)
{
    ScePspFVector3 position, scaling;
    unsigned int format = GU_TEXTURE_32BITF | GU_COLOR_8888 |
                          GU_VERTEX_32BITF | GU_TRANSFORM_3D;
    if (!model || !model->loaded || !model->skinned_vertices) return;

    setup_walk_pose(model, walk_phase, move_blend, attack_combo, attack_timer);
    skin_vertices(model);
    model_texture_bind(texture);

    position.x = x; position.y = y; position.z = z;
    scaling.x = scale; scaling.y = scale; scaling.z = scale;
    sceGumMatrixMode(GU_MODEL);
    sceGumPushMatrix();
    sceGumTranslate(&position);
    if (yaw != 0.0f) sceGumRotateY(yaw);
    sceGumScale(&scaling);
    sceGumUpdateMatrix();
    sceGuDrawArray(GU_TRIANGLES, format, model->vertex_count, NULL,
                   model->skinned_vertices);
    sceGumPopMatrix();
    model_texture_unbind();
}

void rigged_model_free(RiggedModel *model)
{
    if (!model) return;
    if (model->bind_vertices) free(model->bind_vertices);
    if (model->skinned_vertices) free(model->skinned_vertices);
    if (model->influences) free(model->influences);
    if (model->bones) free(model->bones);
    memset(model, 0, sizeof(*model));
}
