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

static float vec3_normalize(float v[3])
{
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len < 0.00001f) return 0.0f;
    v[0] /= len; v[1] /= len; v[2] /= len;
    return len;
}

/* Blend two rigid local transforms without introducing scale/shear. */
static void mat_blend_rigid(float out[12], const float a[12],
                            const float b[12], float t)
{
    float x[3], y[3], z[3], dot;
    x[0] = a[0] + (b[0] - a[0]) * t;
    x[1] = a[4] + (b[4] - a[4]) * t;
    x[2] = a[8] + (b[8] - a[8]) * t;
    y[0] = a[1] + (b[1] - a[1]) * t;
    y[1] = a[5] + (b[5] - a[5]) * t;
    y[2] = a[9] + (b[9] - a[9]) * t;
    if (vec3_normalize(x) < 0.00001f) {
        x[0] = b[0]; x[1] = b[4]; x[2] = b[8];
    }
    dot = x[0]*y[0] + x[1]*y[1] + x[2]*y[2];
    y[0] -= dot*x[0]; y[1] -= dot*x[1]; y[2] -= dot*x[2];
    if (vec3_normalize(y) < 0.00001f) {
        y[0] = b[1]; y[1] = b[5]; y[2] = b[9];
        dot = x[0]*y[0] + x[1]*y[1] + x[2]*y[2];
        y[0] -= dot*x[0]; y[1] -= dot*x[1]; y[2] -= dot*x[2];
        if (vec3_normalize(y) < 0.00001f) {
            y[0] = 0.0f; y[1] = 1.0f; y[2] = 0.0f;
        }
    }
    z[0] = x[1]*y[2] - x[2]*y[1];
    z[1] = x[2]*y[0] - x[0]*y[2];
    z[2] = x[0]*y[1] - x[1]*y[0];
    memcpy(out, b, 12 * sizeof(float));
    out[0] = x[0]; out[4] = x[1]; out[8] = x[2];
    out[1] = y[0]; out[5] = y[1]; out[9] = y[2];
    out[2] = z[0]; out[6] = z[1]; out[10] = z[2];
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

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float smooth01(float t)
{
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/* Wind-up -> contact -> recovery curve, with eased joins. */
static float strike_curve(float t)
{
    if (t < 0.22f) return lerp(0.0f, 0.16f, smooth01(t / 0.22f));
    if (t < 0.50f) return lerp(0.16f, 1.0f, smooth01((t - 0.22f) / 0.28f));
    return lerp(1.0f, 0.0f, smooth01((t - 0.50f) / 0.50f));
}

/* Rodrigues rotation, expressed around an arbitrary bone-local unit axis. */
static void mat_axis_rotation(float out[12], float x, float y, float z, float angle)
{
    float len = sqrtf(x * x + y * y + z * z);
    float c, s, t;
    if (len < 0.00001f || fabsf(angle) < 0.00001f) {
        mat_identity(out);
        return;
    }
    x /= len; y /= len; z /= len;
    c = cosf(angle); s = sinf(angle); t = 1.0f - c;
    mat_identity(out);
    out[0] = t*x*x + c;
    out[1] = t*x*y - s*z;
    out[2] = t*x*z + s*y;
    out[4] = t*x*y + s*z;
    out[5] = t*y*y + c;
    out[6] = t*y*z - s*x;
    out[8] = t*x*z - s*y;
    out[9] = t*y*z + s*x;
    out[10] = t*z*z + c;
}

/*
 * Rotate around a fixed character/model-space axis, not an assumed local X/Y/Z.
 * The SMD importer stores source coordinates; the renderer later maps source
 * (x,y,z) to game (x,z,-y). Therefore:
 *   model right = source +X, model up = source +Z, model forward = source -Y.
 * Transform the requested model-space axis through the transpose of the bone's
 * bind orientation before post-multiplying its local bind matrix.
 */
static void rotate_model_axis(const SkinBone *bone, float local[12],
                              float ax, float ay, float az, float angle)
{
    float lx, ly, lz, r[12], out[12];
    if (fabsf(angle) < 0.00001f) return;
    lx = bone->bind_global[0] * ax + bone->bind_global[4] * ay +
         bone->bind_global[8] * az;
    ly = bone->bind_global[1] * ax + bone->bind_global[5] * ay +
         bone->bind_global[9] * az;
    lz = bone->bind_global[2] * ax + bone->bind_global[6] * ay +
         bone->bind_global[10] * az;
    mat_axis_rotation(r, lx, ly, lz, angle);
    mat_mul(out, local, r);
    memcpy(local, out, sizeof(out));
}

static int name_is(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

/* Side sign comes from the extracted rig: L joints are on +source-X, R on -X. */
static float joint_side(const char *name)
{
    return name[0] == 'L' ? 1.0f : -1.0f;
}

static void pose_bone(const SkinBone *bone, float local[12], const SkinPose *pose)
{
    const char *n = bone->name;
    const float up_x = 0.0f, up_y = 0.0f, up_z = 1.0f;
    const float lateral_x = 1.0f, lateral_y = 0.0f, lateral_z = 0.0f;
    const float depth_x = 0.0f, depth_y = 1.0f, depth_z = 0.0f;
    float gait = sinf(pose->phase * 0.55f);
    /* Contact is frame 5 for every combo step; align the pose peak to impact. */
    float strike = strike_curve((float)pose->timer / 12.0f);
    float air = clamp01(pose->height / 2.35f);
    float rising = clamp01(pose->vertical_velocity / 0.24f);
    float falling = clamp01(-pose->vertical_velocity / 0.18f);
    float side, yaw = 0.0f, elbow = 0.0f, lift = 0.0f;
    float thigh = 0.0f, knee = 0.0f, foot = 0.0f, abduct = 0.0f;
    float attack_body = strike;
    int attacking = pose->action == SKIN_ACTION_ATTACK;
    int running = pose->action == SKIN_ACTION_RUN;

    if (name_is(n, "hip")) {
        if (running) {
            /* Lower the pelvis as well as pitching the torso: a real sprint crouch. */
            local[11] -= 0.11f;
            rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, 0.07f * gait);
        } else if (attacking) {
            float twist = 0.0f;
            if (pose->combo == 1) twist = -0.08f;
            else if (pose->combo == 2) twist = 0.18f * attack_body;
            else if (pose->combo == 3) twist = -0.18f * attack_body;
            else if (pose->combo == 4) twist = 0.22f * attack_body;
            rotate_model_axis(bone, local, up_x, up_y, up_z, twist);
        } else if (pose->action == SKIN_ACTION_HIT) {
            rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, -0.08f);
        }
        return;
    }

    if (name_is(n, "waist")) {
        float lean = 0.0f, twist = 0.0f;
        if (running) lean = 0.34f;
        else if (pose->action == SKIN_ACTION_DASH) lean = 0.38f;
        else if (pose->action == SKIN_ACTION_JUMP) lean = -0.04f * air + 0.10f * falling;
        else if (pose->action == SKIN_ACTION_GUARD || pose->action == SKIN_ACTION_IDLE) lean = 0.10f;
        else if (attacking) lean = 0.05f * attack_body;
        else if (pose->action == SKIN_ACTION_HIT) lean = -0.18f;
        if (attacking) {
            if (pose->combo == 1) twist = -0.13f * attack_body;
            else if (pose->combo == 2) twist = 0.22f * attack_body;
            else if (pose->combo == 3) twist = -0.28f * attack_body;
            else if (pose->combo == 4) twist = 0.24f * attack_body;
        }
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, lean);
        rotate_model_axis(bone, local, up_x, up_y, up_z, twist);
        return;
    }

    if (name_is(n, "breast")) {
        float lean = 0.0f, twist = 0.0f;
        if (running) lean = 0.43f;
        else if (pose->action == SKIN_ACTION_DASH) lean = 0.42f;
        else if (pose->action == SKIN_ACTION_JUMP) lean = -0.08f * air + 0.06f * falling;
        else if (pose->action == SKIN_ACTION_GUARD || pose->action == SKIN_ACTION_IDLE) lean = 0.13f;
        else if (attacking) lean = 0.10f * attack_body;
        else if (pose->action == SKIN_ACTION_HIT) lean = -0.24f;
        else lean = sinf(pose->phase * 0.11f) * 0.012f;
        if (attacking) {
            if (pose->combo == 1) twist = -0.18f * attack_body;
            else if (pose->combo == 2) twist = 0.32f * attack_body;
            else if (pose->combo == 3) twist = -0.40f * attack_body;
            else if (pose->combo == 4) twist = 0.30f * attack_body;
        }
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, lean);
        rotate_model_axis(bone, local, up_x, up_y, up_z, twist);
        return;
    }

    if (name_is(n, "neck") || name_is(n, "head")) {
        float pitch = 0.0f, yaw_head = 0.0f;
        if (running) pitch = -0.13f;
        else if (pose->action == SKIN_ACTION_DASH) pitch = -0.18f;
        else if (pose->action == SKIN_ACTION_JUMP) pitch = 0.04f * air;
        else if (attacking) pitch = -0.10f * attack_body;
        else if (pose->action == SKIN_ACTION_HIT) pitch = 0.18f;
        if (name_is(n, "head")) pitch *= 0.70f;
        if (attacking) yaw_head = (pose->combo_dir > 0 ? -0.08f : 0.08f) * attack_body;
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, pitch);
        rotate_model_axis(bone, local, up_x, up_y, up_z, yaw_head);
        return;
    }

    if (name_is(n, "L_collar") || name_is(n, "R_collar")) {
        side = joint_side(n);
        if (running) {
            rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, 0.06f);
            rotate_model_axis(bone, local, depth_x, depth_y, depth_z, side * 0.12f);
        } else if (pose->action == SKIN_ACTION_GUARD || pose->action == SKIN_ACTION_IDLE) {
            /* Positive side-signed depth rotation lowers both T-pose arms. */
            rotate_model_axis(bone, local, depth_x, depth_y, depth_z, side * 0.22f);
            rotate_model_axis(bone, local, up_x, up_y, up_z, -side * 0.06f);
        } else if (attacking) {
            rotate_model_axis(bone, local, up_x, up_y, up_z,
                              -side * 0.10f * attack_body);
            rotate_model_axis(bone, local, depth_x, depth_y, depth_z,
                              side * 0.12f * attack_body);
        }
        return;
    }

    if (name_is(n, "L_harm") || name_is(n, "R_harm") ||
        name_is(n, "L_harmEX") || name_is(n, "R_harmEX")) {
        side = joint_side(n);
        if (running) {
            /* Classic Naruto ninja run: both upper arms trail behind the torso. */
            yaw = side * (1.28f + 0.055f * gait);
            lift = side * (0.06f + 0.025f * gait);
        } else if (pose->action == SKIN_ACTION_GUARD || pose->action == SKIN_ACTION_IDLE) {
            /* Ready-to-fight silhouette: shoulders down, fists in front, elbows bent. */
            yaw = -side * 0.58f;
            lift = side * 0.62f;
        } else if (pose->action == SKIN_ACTION_JUMP) {
            yaw = -side * 0.42f;
            lift = -side * (0.46f + 0.20f * air + 0.16f * rising - 0.10f * falling);
        } else if (pose->action == SKIN_ACTION_DASH) {
            yaw = side * 1.08f;
            lift = 0.10f;
        } else if (pose->action == SKIN_ACTION_JUTSU) {
            yaw = -side * (0.46f + 0.05f * sinf((float)pose->timer * 0.16f));
            lift = -side * 0.12f;
        } else if (pose->action == SKIN_ACTION_HIT) {
            yaw = side * 0.34f;
            lift = -side * 0.18f;
        } else if (attacking) {
            int lead_strike = (pose->combo == 1 || pose->combo == 3) && side > 0.0f;
            int rear_strike = pose->combo == 2 && side < 0.0f;
            if (lead_strike) {
                /* Jab (1) and bent lead hook (3). */
                yaw = -side * (pose->combo == 3 ? 0.62f + 0.72f * strike
                                                : 0.30f + 0.98f * strike);
                lift = side * (pose->combo == 3 ? 0.32f + 0.22f * strike
                                               : 0.14f + 0.18f * strike);
            } else if (rear_strike) {
                /* Rear cross: torso turns through the shoulder, not just the hand. */
                yaw = -side * (0.42f + 1.12f * strike);
                lift = side * (0.18f + 0.20f * strike);
            } else {
                /* Non-striking hand stays up in guard; combo four is the side kick. */
                yaw = -side * 0.58f;
                lift = side * 0.58f;
            }
        } else {
            yaw = -side * 0.58f;
            lift = side * 0.62f;
        }
        if (strstr(n, "EX") != NULL) { yaw *= 0.45f; lift *= 0.45f; }
        rotate_model_axis(bone, local, up_x, up_y, up_z, yaw);
        rotate_model_axis(bone, local, depth_x, depth_y, depth_z, lift);
        return;
    }

    if (name_is(n, "L_larm") || name_is(n, "R_larm") ||
        name_is(n, "L_larmEX") || name_is(n, "R_larmEX")) {
        side = joint_side(n);
        if (running) elbow = side * (0.18f + 0.05f * gait);
        else if (pose->action == SKIN_ACTION_GUARD || pose->action == SKIN_ACTION_IDLE)
            elbow = -side * 0.88f;
        else if (pose->action == SKIN_ACTION_JUMP) elbow = -side * (0.34f + 0.28f * air + 0.10f * falling);
        else if (pose->action == SKIN_ACTION_DASH) elbow = side * 0.10f;
        else if (pose->action == SKIN_ACTION_JUTSU) elbow = -side * 0.72f;
        else if (pose->action == SKIN_ACTION_HIT) elbow = side * 0.20f;
        else if (attacking) {
            int jab_arm = (pose->combo == 1 || pose->combo == 3) && side > 0.0f;
            int cross_arm = pose->combo == 2 && side < 0.0f;
            if (jab_arm && pose->combo == 3)
                elbow = -side * (1.06f - 0.12f * strike); /* hook stays bent */
            else if (jab_arm || cross_arm)
                elbow = -side * (0.92f - 0.72f * strike); /* straight punches extend */
            else
                elbow = -side * 0.88f;
        } else elbow = -side * 0.88f;
        if (strstr(n, "EX") != NULL) elbow *= 0.45f;
        rotate_model_axis(bone, local, up_x, up_y, up_z, elbow);
        return;
    }

    if (name_is(n, "L_hand") || name_is(n, "R_hand")) {
        side = joint_side(n);
        if (running) rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, -0.035f);
        else if (attacking) rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z,
                                               -side * 0.10f * strike);
        else if (pose->action == SKIN_ACTION_GUARD)
            rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, -side * 0.08f);
        return;
    }

    if (strstr(n, "_fin") != NULL) {
        float curl = 0.0f;
        if (pose->action == SKIN_ACTION_IDLE) curl = 0.08f;
        else if (pose->action == SKIN_ACTION_GUARD || attacking) curl = 0.20f;
        else if (pose->action == SKIN_ACTION_JUTSU) curl = 0.12f;
        else if (pose->action == SKIN_ACTION_RUN) curl = 0.015f; /* open, trailing hands */
        if (n[0] == 'R') curl = -curl;
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, curl);
        return;
    }

    if (name_is(n, "L_hleg") || name_is(n, "R_hleg") ||
        name_is(n, "L_hlegEX") || name_is(n, "R_hlegEX")) {
        side = joint_side(n);
        if (running) thigh = -side * 0.76f * gait;
        else if (pose->action == SKIN_ACTION_JUMP) thigh = -side * (0.22f + 0.35f * air + 0.10f * rising - 0.08f * falling);
        else if (pose->action == SKIN_ACTION_DASH) thigh = -side * 0.30f;
        else if (pose->action == SKIN_ACTION_HIT) thigh = side * 0.12f;
        else if (attacking && pose->combo == 4) {
            /* Chamber and lift the right leg outward for the finishing side kick. */
            thigh = side < 0.0f ? -0.10f * strike : 0.0f;
            abduct = side < 0.0f ? 1.02f * strike : 0.0f;
        } else if (attacking) thigh = -side * 0.04f * strike;
        else thigh = -side * 0.035f;
        if (strstr(n, "EX") != NULL) { thigh *= 0.55f; abduct *= 0.55f; }
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, thigh);
        if (abduct != 0.0f)
            rotate_model_axis(bone, local, depth_x, depth_y, depth_z, abduct);
        return;
    }

    if (name_is(n, "L_lleg") || name_is(n, "R_lleg")) {
        side = joint_side(n);
        if (running) knee = 0.34f + 0.96f * fmaxf(0.0f, -side * gait);
        else if (pose->action == SKIN_ACTION_JUMP) knee = 0.34f + 0.34f * air + 0.22f * falling;
        else if (attacking && pose->combo == 4 && side < 0.0f)
            knee = lerp(0.98f, 0.10f, strike);
        else if (pose->action == SKIN_ACTION_HIT) knee = 0.20f;
        else knee = 0.12f;
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, knee);
        return;
    }

    if (name_is(n, "L_foot") || name_is(n, "R_foot")) {
        side = joint_side(n);
        if (running) {
            thigh = -side * 0.76f * gait;
            knee = 0.34f + 0.96f * fmaxf(0.0f, -side * gait);
            foot = -0.34f * thigh - 0.30f * knee;
        } else if (pose->action == SKIN_ACTION_JUMP) {
            thigh = -side * (0.22f + 0.35f * air + 0.10f * rising - 0.08f * falling);
            knee = 0.34f + 0.34f * air + 0.22f * falling;
            foot = -0.30f * thigh - 0.55f * knee;
        } else if (attacking && pose->combo == 4 && side < 0.0f) {
            foot = -0.16f + 0.22f * strike;
        } else foot = -0.06f;
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, foot);
        return;
    }

    if (name_is(n, "L_toe") || name_is(n, "R_toe")) {
        side = joint_side(n);
        if (running) {
            thigh = -side * 0.76f * gait;
            knee = 0.34f + 0.96f * fmaxf(0.0f, -side * gait);
            foot = -0.34f * thigh - 0.30f * knee;
        } else foot = 0.0f;
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, foot * 0.55f);
        return;
    }

    /* Hair and tail strands use tiny secondary motion; face/attachment nodes stay bound. */
    if (strstr(n, "hr") != NULL && running) {
        float phase = pose->phase * 0.55f + (float)bone->parent * 0.07f;
        rotate_model_axis(bone, local, lateral_x, lateral_y, lateral_z, sinf(phase) * 0.045f);
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
        memcpy(mesh->bones[i].bind_global, bind_global[i], sizeof(bind_global[i]));
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

    if (!instance->pose_valid) {
        instance->last_pose = *pose;
        instance->transition_pose = *pose;
        instance->pose_valid = 1;
        instance->blend_frames_left = 0;
    } else {
        if (pose->action != instance->last_pose.action ||
            pose->combo != instance->last_pose.combo ||
            pose->combo_dir != instance->last_pose.combo_dir) {
            instance->transition_pose = instance->last_pose;
            instance->blend_frames_left = 5;
        }
        instance->last_pose = *pose;
    }

    {
        float blend = 1.0f;
        if (instance->blend_frames_left > 0) {
            blend = 1.0f - (float)instance->blend_frames_left / 5.0f;
            instance->blend_frames_left--;
        }
        for (i = 0; i < mesh->bone_count; i++) {
            float local[12];
            const SkinBone *bone = &mesh->bones[i];
            memcpy(local, bone->local_bind, sizeof(local));
            pose_bone(bone, local, pose);
            if (blend < 1.0f) {
                float previous[12];
                memcpy(previous, bone->local_bind, sizeof(previous));
                pose_bone(bone, previous, &instance->transition_pose);
                mat_blend_rigid(local, previous, local, blend);
            }
            if (bone->parent < 0)
            memcpy(instance->global[i], local, sizeof(local));
        else
            mat_mul(instance->global[i], instance->global[bone->parent], local);
            mat_mul(instance->skin[i], instance->global[i], bone->inverse_bind);
        }
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
