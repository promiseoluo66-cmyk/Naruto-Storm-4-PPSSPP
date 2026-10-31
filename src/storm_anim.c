#include "storm_anim.h"
#include <math.h>

static float clamp01(float t)
{
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

/* Cosine ease-in/ease-out. */
static float ease_cos(float t)
{
    t = clamp01(t);
    return 0.5f - 0.5f * cosf(t * 3.14159265358979323846f);
}

static float segment(float t, float a, float b, float va, float vb)
{
    if (b <= a) return vb;
    return va + (vb - va) * ease_cos((t - a) / (b - a));
}

/* Four pose keys: chamber -> strike -> small overshoot/recoil -> recovery. */
static float track(float t, float v0, float v1, float v2, float v3)
{
    if (t < 0.20f) return segment(t, 0.0f, 0.20f, v0, v1);
    if (t < 0.56f) return segment(t, 0.20f, 0.56f, v1, v2);
    return segment(t, 0.56f, 1.0f, v2, v3);
}

void storm_anim_evaluate(StormPose *p, int attacking, int running, int combo,
                         int timer, int direction, float phase)
{
    float gait;
    float t;
    if (!p) return;

    p->spine_lean = 0.0f;
    p->r_arm_forward = 0.0f;
    p->l_arm_forward = 0.0f;
    p->r_arm_lift = 0.0f;
    p->l_arm_lift = 0.0f;
    gait = sinf(phase) * 0.13f;
    p->r_leg_forward = gait;
    p->l_leg_forward = -gait;
    p->r_leg_lift = fmaxf(0.0f, sinf(phase)) * 0.10f;
    p->l_leg_lift = fmaxf(0.0f, -sinf(phase)) * 0.10f;

    if (running && !attacking) {
        /* Forward torso pitch; arm trail is rendered by the fighter rig. */
        p->spine_lean = 1.05f;
        p->r_arm_forward = -1.02f;
        p->l_arm_forward = -1.02f;
        p->r_arm_lift = sinf(phase * 2.6f) * 0.055f;
        p->l_arm_lift = -p->r_arm_lift;
        return;
    }
    if (!attacking) return;

    p->r_leg_forward = p->l_leg_forward = 0.0f;
    p->r_leg_lift = p->l_leg_lift = 0.0f;
    t = clamp01((float)timer / (combo == 4 ? 18.0f : 12.0f));

    if (direction > 0) {
        /* Up input: crouch/chamber, upward launcher, recoil and settle. */
        p->r_arm_forward = track(t, 0.0f, -0.18f, 0.88f, 0.08f);
        p->r_arm_lift = track(t, 0.0f, -0.28f, 0.82f, 0.0f);
        p->l_arm_forward = track(t, 0.0f, 0.05f, 0.35f, 0.0f);
        p->l_arm_lift = track(t, 0.0f, -0.12f, 0.42f, 0.0f);
        p->r_leg_lift = track(t, 0.0f, 0.08f, 0.20f, 0.0f);
    } else if (direction < 0) {
        /* Down input: lift the leg, drive it down, then recover. */
        p->r_leg_forward = track(t, 0.0f, 0.05f, 0.95f, 0.12f);
        p->r_leg_lift = track(t, 0.0f, 0.95f, -0.18f, 0.0f);
        p->r_arm_forward = track(t, 0.0f, -0.12f, 0.20f, 0.0f);
        p->l_arm_forward = track(t, 0.0f, 0.10f, 0.25f, 0.0f);
    } else if (combo == 1) {
        p->r_arm_forward = track(t, 0.0f, -0.22f, 1.12f, 0.06f);
        p->r_arm_lift = track(t, 0.0f, -0.08f, 0.12f, 0.0f);
        p->l_arm_forward = track(t, 0.0f, 0.12f, 0.28f, 0.0f);
    } else if (combo == 2) {
        p->l_arm_forward = track(t, 0.0f, -0.18f, 0.98f, 0.05f);
        p->l_arm_lift = track(t, 0.0f, 0.08f, 0.30f, 0.0f);
        p->r_arm_forward = track(t, 0.0f, 0.12f, 0.24f, 0.0f);
    } else if (combo == 3) {
        p->r_leg_forward = track(t, 0.0f, 0.05f, 1.05f, 0.10f);
        p->r_leg_lift = track(t, 0.0f, 0.78f, 0.22f, 0.0f);
        p->l_arm_forward = track(t, 0.0f, 0.12f, 0.45f, 0.0f);
        p->r_arm_forward = track(t, 0.0f, -0.12f, 0.15f, 0.0f);
    } else {
        /* Heavy finisher: both arms drive forward, then recoil. */
        p->r_arm_forward = track(t, 0.0f, -0.30f, 1.20f, 0.0f);
        p->l_arm_forward = track(t, 0.0f, -0.30f, 1.20f, 0.0f);
        p->r_arm_lift = track(t, 0.0f, -0.10f, 0.18f, 0.0f);
        p->l_arm_lift = p->r_arm_lift;
        p->r_leg_forward = track(t, 0.0f, -0.08f, 0.28f, 0.0f);
    }
}
