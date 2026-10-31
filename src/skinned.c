#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspgu.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "skinned.h"

#define SKIN_FILE_MAX_BYTES (8u * 1024u * 1024u)

typedef struct {
    char magic[4];
    unsigned int version;
    unsigned int bone_count;
    unsigned int vertex_count;
    unsigned int source_frame_count;
} SkinHeader;

typedef struct {
    int parent;
    char name[32];
    float local_bind[12];
} SkinBoneDisk;

static int read_exact(SceUID fd, void *dst, unsigned int bytes)
{
    return sceIoRead(fd, dst, bytes) == (int)bytes ? 0 : -1;
}

static void mat_identity(float m[12])
{
    memset(m, 0, 12 * sizeof(float));
    m[0] = m[5] = m[10] = 1.0f;
}

static void mat_mul(float out[12], const float a[12], const float b[12])
{
    float r[12];
    int row, col;
    for (row = 0; row < 3; row++) {
        for (col = 0; col < 3; col++)
            r[row * 4 + col] =
                a[row * 4 + 0] * b[0 * 4 + col] +
                a[row * 4 + 1] * b[1 * 4 + col] +
                a[row * 4 + 2] * b[2 * 4 + col];
        r[row * 4 + 3] =
            a[row * 4 + 0] * b[3] +
            a[row * 4 + 1] * b[7] +
            a[row * 4 + 2] * b[11] +
            a[row * 4 + 3];
    }
    memcpy(out, r, sizeof(r));
}

static void mat_inverse_rigid(float out[12], const float m[12])
{
    /* Inverse of a rotation + translation matrix. */
    out[0] = m[0]; out[1] = m[4]; out[2] = m[8];
    out[4] = m[1]; out[5] = m[5]; out[6] = m[9];
    out[8] = m[2]; out[9] = m[6]; out[10] = m[10];
    out[3] = -(out[0] * m[3] + out[1] * m[7] + out[2] * m[11]);
    out[7] = -(out[4] * m[3] + out[5] * m[7] + out[6] * m[11]);
    out[11] = -(out[8] * m[3] + out[9] * m[7] + out[10] * m[11]);
}

static void mat_rotation(float out[12], int axis, float angle)
{
    float c = cosf(angle), s = sinf(angle);
    mat_identity(out);
    if (axis == 0) {
        out[5] = c; out[6] = -s;
        out[9] = s; out[10] = c;
    } else if (axis == 1) {
        out[0] = c; out[2] = s;
        out[8] = -s; out[10] = c;
    } else {
        out[0] = c; out[1] = -s;
        out[4] = s; out[5] = c;
    }
}

static void rotate_local(float m[12], int axis, float angle)
{
    float r[12], out[12];
    if (fabsf(angle) < 0.00001f) return;
    mat_rotation(r, axis, angle);
    mat_mul(out, m, r);
    memcpy(m, out, sizeof(out));
}

static int name_is(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static void pose_bone(const SkinBone *bone, float local[12], const SkinPose *pose)
{
    const char *n = bone->name;
    float wave = sinf(pose->phase * 2.6f);
    float t = (float)pose->timer;
    float pulse = sinf(fminf(t / 12.0f, 1.0f) * 3.14159265f);
    float lead = ((pose->combo + pose->combo_dir) & 1) ? -1.0f : 1.0f;
    float a = 0.0f;

    if (name_is(n, "L_harm") || name_is(n, "R_harm")) {
        float side = name_is(n, "L_harm") ? -1.0f : 1.0f;
        if (pose->action == SKIN_ACTION_RUN) a = side * 0.52f * wave;
        else if (pose->action == SKIN_ACTION_ATTACK)
            a = side * lead * 1.15f * pulse;
        else if (pose->action == SKIN_ACTION_JUTSU) a = side * 0.85f;
        else if (pose->action == SKIN_ACTION_JUMP) a = side * 0.90f;
        else if (pose->action == SKIN_ACTION_GUARD) a = side * 0.62f;
        else if (pose->action == SKIN_ACTION_DASH) a = -side * 0.85f;
        rotate_local(local, 2, a);
    } else if (name_is(n, "L_larm") || name_is(n, "R_larm")) {
        float side = name_is(n, "L_larm") ? -1.0f : 1.0f;
        if (pose->action == SKIN_ACTION_RUN) a = 0.15f + 0.18f * wave;
        else if (pose->action == SKIN_ACTION_ATTACK) a = -lead * 0.95f * pulse;
        else if (pose->action == SKIN_ACTION_JUTSU) a = -0.95f;
        else if (pose->action == SKIN_ACTION_JUMP) a = -0.35f;
        else if (pose->action == SKIN_ACTION_GUARD) a = -1.05f;
        else if (pose->action == SKIN_ACTION_DASH) a = 0.15f;
        rotate_local(local, 2, a * side);
    } else if (name_is(n, "L_hleg") || name_is(n, "R_hleg")) {
        float side = name_is(n, "L_hleg") ? -1.0f : 1.0f;
        if (pose->action == SKIN_ACTION_RUN) a = side * 0.48f * wave;
        else if (pose->action == SKIN_ACTION_JUMP) a = side * 0.28f;
        else if (pose->action == SKIN_ACTION_ATTACK) a = side * 0.12f * pulse;
        rotate_local(local, 0, a);
    } else if (name_is(n, "L_lleg") || name_is(n, "R_lleg")) {
        float side = name_is(n, "L_lleg") ? -1.0f : 1.0f;
        if (pose->action == SKIN_ACTION_RUN) a = 0.52f + fmaxf(0.0f, -side * wave) * 0.45f;
        else if (pose->action == SKIN_ACTION_JUMP) a = 0.72f;
        rotate_local(local, 0, a);
    } else if (name_is(n, "breast")) {
        if (pose->action == SKIN_ACTION_IDLE) a = sinf(pose->phase * 0.055f) * 0.018f;
        else if (pose->action == SKIN_ACTION_RUN) a = -0.12f;
        else if (pose->action == SKIN_ACTION_ATTACK) a = lead * 0.22f * pulse;
        else if (pose->action == SKIN_ACTION_DASH) a = -0.30f;
        rotate_local(local, 0, a);
    } else if (name_is(n, "waist")) {
        if (pose->action == SKIN_ACTION_RUN) a = -0.08f;
        else if (pose->action == SKIN_ACTION_ATTACK) a = lead * 0.13f * pulse;
        else if (pose->action == SKIN_ACTION_DASH) a = -0.18f;
        rotate_local(local, 0, a);
    } else if (name_is(n, "head")) {
        if (pose->action == SKIN_ACTION_IDLE) a = sinf(pose->phase * 0.037f) * 0.025f;
        else if (pose->action == SKIN_ACTION_ATTACK) a = -lead * 0.16f * pulse;
        rotate_local(local, 0, a);
    }
}

int skinned_mesh_load(SkinnedMesh *mesh, const char *mesh_path,
                      const char *texture_path)
{
    SceUID fd;
    SkinHeader header;
    unsigned int i;
    unsigned int expected;
    float bind_global[SKIN_MAX_BONES][12];

    if (!mesh || !mesh_path || !texture_path) return -1;
    memset(mesh, 0, sizeof(*mesh));

    fd = sceIoOpen(mesh_path, PSP_O_RDONLY, 0);
    if (fd < 0) return -2;
    if (read_exact(fd, &header, sizeof(header)) < 0) {
        sceIoClose(fd);
        return -3;
    }
    if (memcmp(header.magic, "NSKM", 4) != 0 || header.version != 1 ||
        header.bone_count == 0 || header.bone_count > SKIN_MAX_BONES ||
        header.vertex_count == 0 || header.vertex_count > 60000 ||
        (header.vertex_count % 3) != 0) {
        sceIoClose(fd);
        return -4;
    }
    expected = sizeof(header) + header.bone_count * sizeof(SkinBoneDisk) +
               header.vertex_count * sizeof(SkinVertex);
    if (expected > SKIN_FILE_MAX_BYTES) {
        sceIoClose(fd);
        return -5;
    }

    mesh->bones = (SkinBone *)memalign(16, header.bone_count * sizeof(SkinBone));
    mesh->vertices = (SkinVertex *)memalign(16, header.vertex_count * sizeof(SkinVertex));
    if (!mesh->bones || !mesh->vertices) {
        sceIoClose(fd);
        skinned_mesh_free(mesh);
        return -6;
    }

    for (i = 0; i < header.bone_count; i++) {
        SkinBoneDisk disk;
        if (read_exact(fd, &disk, sizeof(disk)) < 0) {
            sceIoClose(fd);
            skinned_mesh_free(mesh);
            return -7;
        }
        mesh->bones[i].parent = disk.parent;
        memcpy(mesh->bones[i].name, disk.name, sizeof(disk.name));
        mesh->bones[i].name[sizeof(mesh->bones[i].name) - 1] = '\0';
        memcpy(mesh->bones[i].local_bind, disk.local_bind, sizeof(disk.local_bind));
        if (disk.parent >= (int)i || disk.parent < -1) {
            sceIoClose(fd);
            skinned_mesh_free(mesh);
            return -8;
        }
        if (disk.parent < 0)
            memcpy(bind_global[i], disk.local_bind, sizeof(disk.local_bind));
        else
            mat_mul(bind_global[i], bind_global[disk.parent], disk.local_bind);
        mat_inverse_rigid(mesh->bones[i].inverse_bind, bind_global[i]);
    }

    if (read_exact(fd, mesh->vertices,
                   header.vertex_count * sizeof(SkinVertex)) < 0) {
        sceIoClose(fd);
        skinned_mesh_free(mesh);
        return -9;
    }
    sceIoClose(fd);

    mesh->bone_count = header.bone_count;
    mesh->vertex_count = header.vertex_count;
    for (i = 0; i < mesh->vertex_count; i++) {
        int k;
        float total = 0.0f;
        for (k = 0; k < 4; k++) {
            if (mesh->vertices[i].weight[k] > 0.0f) {
                if (mesh->vertices[i].bone[k] >= mesh->bone_count) {
                    skinned_mesh_free(mesh);
                    return -10;
                }
                total += mesh->vertices[i].weight[k];
            }
        }
        if (total <= 0.00001f) {
            mesh->vertices[i].bone[0] = 0;
            mesh->vertices[i].weight[0] = 1.0f;
            mesh->vertices[i].weight[1] = mesh->vertices[i].weight[2] =
                mesh->vertices[i].weight[3] = 0.0f;
        } else {
            for (k = 0; k < 4; k++) mesh->vertices[i].weight[k] /= total;
        }
    }

    if (model_texture_load(&mesh->texture, texture_path) < 0) {
        skinned_mesh_free(mesh);
        return -11;
    }
    sceKernelDcacheWritebackInvalidateRange(mesh->bones,
                                             mesh->bone_count * sizeof(SkinBone));
    sceKernelDcacheWritebackInvalidateRange(mesh->vertices,
                                             mesh->vertex_count * sizeof(SkinVertex));
    return 0;
}

int skinned_instance_init(SkinnedInstance *instance, const SkinnedMesh *mesh)
{
    if (!instance || !mesh || mesh->vertex_count == 0) return -1;
    memset(instance, 0, sizeof(*instance));
    instance->vertices = (ModelVertex *)memalign(16,
                          mesh->vertex_count * sizeof(ModelVertex));
    if (!instance->vertices) return -2;
    memset(instance->vertices, 0, mesh->vertex_count * sizeof(ModelVertex));
    return 0;
}

void skinned_instance_free(SkinnedInstance *instance)
{
    if (!instance) return;
    if (instance->vertices) free(instance->vertices);
    memset(instance, 0, sizeof(*instance));
}

void skinned_mesh_free(SkinnedMesh *mesh)
{
    if (!mesh) return;
    if (mesh->bones) free(mesh->bones);
    if (mesh->vertices) free(mesh->vertices);
    model_texture_free(&mesh->texture);
    memset(mesh, 0, sizeof(*mesh));
}

void skinned_draw(const SkinnedMesh *mesh, SkinnedInstance *instance,
                  float x, float y, float z, float yaw, float scale,
                  const SkinPose *pose)
{
    unsigned int i;
    Model draw_model;
    SkinPose idle_pose;

    if (!mesh || !instance || !instance->vertices || !mesh->bones ||
        !mesh->vertices || mesh->bone_count == 0) return;
    if (!pose) {
        memset(&idle_pose, 0, sizeof(idle_pose));
        idle_pose.action = SKIN_ACTION_IDLE;
        idle_pose.phase = 0.0f;
        pose = &idle_pose;
    }

    for (i = 0; i < mesh->bone_count; i++) {
        float local[12];
        const SkinBone *bone = &mesh->bones[i];
        memcpy(local, bone->local_bind, sizeof(local));
        pose_bone(bone, local, pose);
        if (bone->parent < 0)
            memcpy(instance->global[i], local, sizeof(local));
        else
            mat_mul(instance->global[i], instance->global[bone->parent], local);
        mat_mul(instance->skin[i], instance->global[i], bone->inverse_bind);
    }

    for (i = 0; i < mesh->vertex_count; i++) {
        const SkinVertex *src = &mesh->vertices[i];
        float px = 0.0f, py = 0.0f, pz = 0.0f;
        int k;
        for (k = 0; k < 4; k++) {
            float w = src->weight[k];
            const float *m;
            float tx, ty, tz;
            unsigned int bi;
            if (w <= 0.00001f) continue;
            bi = src->bone[k];
            m = instance->skin[bi];
            tx = m[0] * src->x + m[1] * src->y + m[2] * src->z + m[3];
            ty = m[4] * src->x + m[5] * src->y + m[6] * src->z + m[7];
            tz = m[8] * src->x + m[9] * src->y + m[10] * src->z + m[11];
            px += tx * w; py += ty * w; pz += tz * w;
        }
        /* The source SMD uses Z-up; the PSP game renderer uses Y-up. */
        instance->vertices[i].x = px * scale;
        instance->vertices[i].y = pz * scale;
        instance->vertices[i].z = -py * scale;
        instance->vertices[i].u = src->u;
        instance->vertices[i].v = src->v;
        instance->vertices[i].color = 0xFFFFFFFFu;
    }

    sceKernelDcacheWritebackInvalidateRange(instance->vertices,
                            mesh->vertex_count * sizeof(ModelVertex));
    draw_model.vertices = instance->vertices;
    draw_model.vertex_count = mesh->vertex_count;
    model_draw_textured(&draw_model, &mesh->texture, x, y, z, yaw);
}
