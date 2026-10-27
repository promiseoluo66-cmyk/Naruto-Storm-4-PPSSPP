/*
 * Naruto Storm 4 PPSSPP - Phase 3
 * Character-specific jutsu (12 move types), Ultimate Jutsu with cinematic camera,
 * awakening/transformation framework, burn (Amaterasu), healing, teleports.
 *
 * Models are placeholder boxes. Move data is compiled in; JSON loading comes later.
 */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"

PSP_MODULE_INFO("NarutoStorm4", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define ARENA_R 9.5f
#define MAXP 10
#define AWAKEN_FRAMES 1800

/* ---------------- exit callback ---------------- */
static int exit_cb(int a1, int a2, void *c) { sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp)
{
    int id = sceKernelCreateCallback("Exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void)
{
    int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
}

/* ---------------- roster ---------------- */
typedef enum {
    MV_DASH, MV_MULTI, MV_PROJ, MV_SPREAD, MV_HOMING, MV_BARRAGE,
    MV_GROUND, MV_SLASH, MV_PUSH, MV_WARP, MV_HEAL, MV_DOT
} MoveType;

typedef struct {
    const char *name;
    MoveType type;
    int cost, cd, dmg, stun;
    float speed, range, kb, size;
} Move;

typedef struct {
    const char *name;
    int hp;
    float speed;
    unsigned body, accent, hair, orb;
    Move jut[3];
    const char *ult_name;
    int ult_dmg;
    int has_awk;
    const char *awk_name;
    unsigned awk_col;
} Def;

#define NUM_CHARS 6
static const Def roster[NUM_CHARS] = {
    { "NARUTO", 1000, 0.135f, RGB(245,140,20), RGB(25,50,190), RGB(250,215,60), RGB(80,180,255),
      { { "RASENGAN",      MV_DASH,  25, 240, 120, 20, 0.45f, 2.0f, 0.30f, 0.5f },
        { "SHADOW CLONES", MV_MULTI, 20, 300,  90, 12, 0.00f, 3.0f, 0.12f, 0.0f },
        { "RASENSHURIKEN", MV_PROJ,  45, 540, 180, 22, 0.25f, 0.0f, 0.35f, 1.1f } },
      "TAILED BEAST BOMB", 450, 1, "KURAMA MODE", RGB(255,170,40) },

    { "SASUKE", 950, 0.145f, RGB(235,235,235), RGB(40,50,120), RGB(25,25,35), RGB(140,170,255),
      { { "CHIDORI",       MV_DASH,  25, 240, 125, 20, 0.50f, 2.0f, 0.30f, 0.5f },
        { "FIREBALL",      MV_PROJ,  20, 210,  85, 12, 0.30f, 0.0f, 0.15f, 0.9f },
        { "AMATERASU",     MV_DOT,   40, 600, 110,  6, 0.28f, 0.0f, 0.00f, 0.9f } },
      "INDRA'S ARROW", 440, 1, "SUSANOO", RGB(90,110,255) },

    { "HASHIRAMA", 1150, 0.120f, RGB(150,40,40), RGB(60,60,60), RGB(30,25,20), RGB(60,200,80),
      { { "WOOD STAKES",   MV_SPREAD, 20, 210,  90, 10, 0.35f, 0.0f, 0.10f, 0.35f },
        { "WOOD DRAGON",   MV_HOMING, 35, 420, 150, 18, 0.22f, 0.0f, 0.20f, 0.8f },
        { "WOOD GOLEM",    MV_GROUND, 40, 600, 130, 22, 0.00f, 2.0f, 0.40f, 0.0f } },
      "GREAT BUDDHA", 520, 1, "SAGE MODE", RGB(255,120,60) },

    { "MADARA", 1100, 0.125f, RGB(120,30,40), RGB(70,70,80), RGB(20,20,25), RGB(255,110,30),
      { { "GREAT FIREBALL", MV_PROJ,  25, 240, 110, 14, 0.28f, 0.0f, 0.18f, 1.3f },
        { "SUSANOO SLASH",  MV_SLASH, 35, 420, 140, 22, 0.00f, 4.5f, 0.40f, 0.0f },
        { "ALMIGHTY PUSH",  MV_PUSH,  30, 480,  60, 16, 0.00f, 6.0f, 0.70f, 0.0f } },
      "METEOR DROP", 500, 1, "SIX PATHS", RGB(240,240,255) },

    { "KAKASHI", 900, 0.150f, RGB(50,70,110), RGB(50,60,70), RGB(190,190,200), RGB(160,200,255),
      { { "LIGHTNING BLADE", MV_DASH,    25, 240, 130, 20, 0.50f, 2.0f, 0.30f, 0.5f },
        { "NINKEN",          MV_BARRAGE, 20, 360,  81, 10, 0.30f, 0.0f, 0.10f, 0.4f },
        { "KAMUI WARP",      MV_WARP,    30, 480,  70, 14, 0.00f, 2.5f, 0.25f, 0.0f } },
      "KAMUI BLADE", 420, 0, "NONE", RGB(255,255,255) },

    { "SAKURA", 850, 0.145f, RGB(210,70,90), RGB(240,240,240), RGB(255,150,190), RGB(255,200,230),
      { { "CHAKRA PUNCH", MV_DASH, 25, 270, 130, 22, 0.35f, 2.2f, 0.40f, 0.5f },
        { "GROUND SMASH", MV_PUSH, 25, 360, 100, 18, 0.00f, 4.0f, 0.50f, 0.0f },
        { "PALM HEAL",    MV_HEAL, 35, 720, 120,  0, 0.00f, 0.0f, 0.00f, 0.0f } },
      "HUNDRED SEAL STRIKE", 400, 0, "NONE", RGB(255,255,255) }
};
#define DEF_HASHIRAMA 2
#define DEF_MADARA 3

/* ---------------- fighter ---------------- */
typedef enum { FS_IDLE, FS_ATTACK, FS_JUTSU, FS_CHARGE, FS_DASH, FS_ULT, FS_AWAKEN } FState;

typedef struct {
    int def, is_ai;
    float x, y, z, vy, fx, fz;
    float hp, maxhp, chakra, dmg_mult, spd_mult, awm;
    FState state;
    int timer, combo, combo_win, queued, stun, inv, guarding, dead, hit_flash, phase_lvl;
    int move, hitdone, frozen, burn, awk;
    int jcd[3];
    float kbx, kbz, dashx, dashz, phase, burn_dps, mark_x, mark_z;
    int ai_cd, ai_mode, ai_mode_t, ai_guard, ai_charge;
} Fighter;

typedef struct {
    float ax, ay;
    int atk, jutsu, charge, guard, jump, dash, ult, awaken;
} Input;

typedef struct {
    int active, life, dot, homing, stun;
    float x, y, z, vx, vz, speed, size, dmg, dotdmg, kb;
    unsigned col;
    Fighter *src, *tgt;
} Proj;

static Fighter pl, en;
static Proj projs[MAXP];

/* ---------------- game state ---------------- */
typedef enum { GS_MENU, GS_SELECT, GS_DIALOGUE, GS_BATTLE, GS_RESULT } GameState;
static GameState gstate = GS_MENU;
static int menu_sel = 0, select_idx = 0, story_mode = 0, dlg_idx = 0;
static int intro = 0, ko_timer = 0, paused = 0, player_won = 0, result_sel = 0;
static unsigned frame = 0;
static char msg[48] = "";
static int msg_t = 0;
static float camx, camy, camz, tgx, tgy, tgz;
static float menu_angle = 0.0f;

static const char *dlg_spk[] = { "NARRATOR", "MADARA", "HASHIRAMA", "MADARA", "HASHIRAMA" };
static const char *dlg_txt[] = {
    "Two legends of the shinobi world clash once more at the Valley of the End.",
    "Hashirama... I will show you the power of the Uchiha.",
    "Then I will end this here, Madara. For the sake of the village.",
    "Come, then! Show me what the First Hokage can do!",
    "Get ready. This battle decides everything."
};
#define DLG_COUNT 5

static void set_msg(const char *s) { strncpy(msg, s, 47); msg[47] = 0; msg_t = 120; }

static float dist_xz(float ax, float az, float bx, float bz)
{
    float dx = ax - bx, dz = az - bz;
    return sqrtf(dx * dx + dz * dz);
}

/* ---------------- fighter logic ---------------- */
static void make_fighter(Fighter *f, int def, float x, float z, int is_ai)
{
    memset(f, 0, sizeof(*f));
    f->def = def; f->is_ai = is_ai;
    f->x = x; f->z = z;
    f->maxhp = f->hp = (float)roster[def].hp;
    f->chakra = 50.0f;
    f->dmg_mult = 1.0f; f->spd_mult = 1.0f;
    f->fx = (x < 0) ? 1.0f : -1.0f;
    f->state = FS_IDLE;
}

/* returns 0 = ignored, 1 = hit, 2 = blocked */
static int deal_damage(Fighter *t, Fighter *src, float dmg, int stun, float kb)
{
    float dx = t->x - src->x, dz = t->z - src->z, len = sqrtf(dx * dx + dz * dz);
    if (len < 0.01f) len = 0.01f;
    if (t->dead || t->inv > 0) return 0;
    dmg *= src->dmg_mult;
    if (src->awk > 0) dmg *= 1.3f;
    if (t->awk > 0) dmg *= 0.8f;
    if (t->guarding && stun < 20) {
        t->hp -= dmg * 0.2f;
        t->x += dx / len * 0.3f; t->z += dz / len * 0.3f;
        if (t->hp <= 0) { t->hp = 0; t->dead = 1; }
        return 2;
    }
    t->hp -= dmg;
    if (t->hp <= 0) { t->hp = 0; t->dead = 1; }
    if (src->awk == 0) src->awm = fminf(100.0f, src->awm + dmg * 0.12f);
    if (t->awk == 0) t->awm = fminf(100.0f, t->awm + dmg * 0.08f);
    t->stun = stun; t->kbx = dx / len * kb; t->kbz = dz / len * kb;
    t->state = FS_IDLE; t->combo = 0; t->queued = 0; t->hit_flash = 6;
    return 1;
}

static void melee_hit(Fighter *f, Fighter *o)
{
    static const float dmg[5] = { 0, 18, 18, 22, 40 };
    static const int stn[5] = { 0, 8, 8, 10, 22 };
    static const float kb[5] = { 0, 0.06f, 0.06f, 0.08f, 0.35f };
    int c = f->combo;
    if (c < 1 || c > 4) return;
    if (dist_xz(f->x, f->z, o->x, o->z) <= 2.4f && fabsf(f->y - o->y) < 1.6f) {
        if (deal_damage(o, f, dmg[c], stn[c], kb[c]) == 1) f->chakra = fminf(100.0f, f->chakra + 4.0f);
    }
}

static void spawn_proj(Fighter *f, Fighter *o, const Move *m, float ang, float side, float fwd,
                       int homing, float dmg, float size, int dot)
{
    int i;
    float ca = cosf(ang), sa = sinf(ang), rx = -f->fz, rz = f->fx;
    for (i = 0; i < MAXP; i++) {
        if (!projs[i].active) {
            Proj *p = &projs[i];
            p->active = 1; p->life = 110; p->dot = dot; p->homing = homing;
            p->stun = m->stun; p->kb = m->kb; p->speed = m->speed; p->size = size;
            p->x = f->x + f->fx * fwd + rx * side;
            p->y = f->y + 1.3f;
            p->z = f->z + f->fz * fwd + rz * side;
            p->vx = (f->fx * ca - f->fz * sa) * m->speed;
            p->vz = (f->fx * sa + f->fz * ca) * m->speed;
            p->dmg = dmg;
            p->dotdmg = dot ? (float)m->dmg * 0.85f : 0.0f;
            p->col = dot ? RGB(60, 10, 90) : roster[f->def].orb;
            p->src = f; p->tgt = o;
            return;
        }
    }
}

/* one tick of the active jutsu; f->timer already incremented */
static void jutsu_tick(Fighter *f, Fighter *o, const Move *m)
{
    int t = f->timer;
    float dist = dist_xz(f->x, f->z, o->x, o->z);

    switch (m->type) {
    case MV_DASH:
        if (t >= 6 && t <= 16) {
            if (dist > 1.3f) { f->x += f->fx * m->speed; f->z += f->fz * m->speed; }
            if (!f->hitdone && dist <= m->range) {
                f->hitdone = 1;
                deal_damage(o, f, (float)m->dmg, m->stun, m->kb);
            }
        }
        if (t >= 24) f->state = FS_IDLE;
        break;
    case MV_MULTI:
        if (t == 6) { f->x = o->x - f->fx * 2.0f; f->z = o->z - f->fz * 2.0f; }
        if (t == 12 || t == 18 || t == 24) {
            if (dist_xz(f->x, f->z, o->x, o->z) <= m->range)
                deal_damage(o, f, (float)m->dmg / 3.0f, t == 24 ? m->stun : 6, t == 24 ? m->kb : 0.05f);
        }
        if (t >= 34) f->state = FS_IDLE;
        break;
    case MV_PROJ:
        if (t == 10) spawn_proj(f, o, m, 0.0f, 0.0f, 1.0f, 0, (float)m->dmg, m->size, 0);
        if (t >= 24) f->state = FS_IDLE;
        break;
    case MV_DOT:
        if (t == 10) spawn_proj(f, o, m, 0.0f, 0.0f, 1.0f, 0, (float)m->dmg * 0.15f, m->size, 1);
        if (t >= 24) f->state = FS_IDLE;
        break;
    case MV_SPREAD:
        if (t == 10) {
            spawn_proj(f, o, m, -0.25f, 0.0f, 1.0f, 0, (float)m->dmg / 3.0f, m->size, 0);
            spawn_proj(f, o, m,  0.00f, 0.0f, 1.0f, 0, (float)m->dmg / 3.0f, m->size, 0);
            spawn_proj(f, o, m,  0.25f, 0.0f, 1.0f, 0, (float)m->dmg / 3.0f, m->size, 0);
        }
        if (t >= 24) f->state = FS_IDLE;
        break;
    case MV_HOMING:
        if (t == 10) spawn_proj(f, o, m, 0.0f, 0.0f, 1.0f, 1, (float)m->dmg, m->size, 0);
        if (t >= 26) f->state = FS_IDLE;
        break;
    case MV_BARRAGE:
        if (t == 8)  spawn_proj(f, o, m,  0.35f,  1.0f, 0.5f, 1, (float)m->dmg / 3.0f, m->size, 0);
        if (t == 14) spawn_proj(f, o, m, -0.35f, -1.0f, 0.5f, 1, (float)m->dmg / 3.0f, m->size, 0);
        if (t == 20) spawn_proj(f, o, m,  0.00f,  0.0f, 0.5f, 1, (float)m->dmg / 3.0f, m->size, 0);
        if (t >= 30) f->state = FS_IDLE;
        break;
    case MV_GROUND:
        if (t == 8) { f->mark_x = o->x; f->mark_z = o->z; }
        if (t == 30 && dist_xz(o->x, o->z, f->mark_x, f->mark_z) <= m->range)
            deal_damage(o, f, (float)m->dmg, m->stun, m->kb);
        if (t >= 38) f->state = FS_IDLE;
        break;
    case MV_SLASH:
        if (t == 10 && dist <= m->range) deal_damage(o, f, (float)m->dmg, m->stun, m->kb);
        if (t >= 22) f->state = FS_IDLE;
        break;
    case MV_PUSH:
        if (t == 10 && dist <= m->range) deal_damage(o, f, (float)m->dmg, m->stun, m->kb);
        if (t >= 22) f->state = FS_IDLE;
        break;
    case MV_WARP:
        if (t == 6) { f->x = o->x + f->fx * 1.6f; f->z = o->z + f->fz * 1.6f; }
        if (t == 12 && dist_xz(f->x, f->z, o->x, o->z) <= m->range)
            deal_damage(o, f, (float)m->dmg, m->stun, m->kb);
        if (t >= 22) f->state = FS_IDLE;
        break;
    case MV_HEAL:
        if (t == 30) { f->hp = fminf(f->maxhp, f->hp + (float)m->dmg); }
        if (t >= 40) f->state = FS_IDLE;
        break;
    }
}

static void start_jutsu(Fighter *f, int idx)
{
    const Move *m = &roster[f->def].jut[idx];
    if (f->chakra < (float)m->cost || f->jcd[idx] > 0) return;
    f->chakra -= (float)m->cost;
    f->jcd[idx] = m->cd;
    f->move = idx;
    f->hitdone = 0;
    f->state = FS_JUTSU;
    f->timer = 0;
}

/* ultimate / awakening; returns 1 if the state changed */
static int try_special(Fighter *f, const Input *in)
{
    const Def *d = &roster[f->def];
    if (in->ult && f->chakra >= 100.0f) {
        f->state = FS_ULT; f->timer = 0; f->inv = 0;
        return 1;
    }
    if (in->awaken && f->awm >= 100.0f && d->has_awk && f->awk == 0) {
        f->awk = AWAKEN_FRAMES; f->awm = 0.0f;
        f->state = FS_AWAKEN; f->timer = 0; f->inv = 40;
        set_msg(d->awk_name);
        return 1;
    }
    return 0;
}

static void fighter_update(Fighter *f, Fighter *o, const Input *in)
{
    const Def *d = &roster[f->def];
    float dx = o->x - f->x, dz = o->z - f->z, dist = sqrtf(dx * dx + dz * dz), rx, rz, r;
    int k;

    if (dist < 0.01f) dist = 0.01f;
    f->fx = dx / dist; f->fz = dz / dist;
    rx = -f->fz; rz = f->fx;
    f->guarding = 0;
    if (f->dead) { f->y = 0; return; }
    if (f->burn > 0) {
        f->burn--;
        f->hp -= f->burn_dps;
        if (f->hp <= 0) { f->hp = 0; f->dead = 1; return; }
    }

    if (!f->frozen) {
        if (f->inv > 0) f->inv--;
        for (k = 0; k < 3; k++) if (f->jcd[k] > 0) f->jcd[k]--;
        if (f->hit_flash > 0) f->hit_flash--;
        if (f->combo_win > 0 && --f->combo_win == 0) f->combo = 0;
        f->chakra = fminf(100.0f, f->chakra + (f->awk > 0 ? 0.3f : 0.05f));
        if (f->awk > 0) f->awk--; else f->awm = fminf(100.0f, f->awm + 0.01f);

        if (f->stun > 0) {
            f->stun--;
            f->x += f->kbx; f->z += f->kbz;
            f->kbx *= 0.8f; f->kbz *= 0.8f;
            f->state = FS_IDLE;
        } else {
            switch (f->state) {
            case FS_IDLE: {
                float mx = rx * in->ax + f->fx * in->ay, mz = rz * in->ax + f->fz * in->ay;
                float ml = sqrtf(mx * mx + mz * mz);
                float sp = d->speed * f->spd_mult * (f->awk > 0 ? 1.15f : 1.0f);
                if (ml > 1.0f) { mx /= ml; mz /= ml; ml = 1.0f; }
                if (in->guard) { f->guarding = 1; sp *= 0.4f; }
                f->x += mx * sp; f->z += mz * sp;
                f->phase += ml * 0.35f;
                if (try_special(f, in)) break;
                if (in->jump && f->y <= 0.001f) f->vy = 0.24f;
                if (in->charge && !in->guard) { f->state = FS_CHARGE; f->timer = 0; }
                else if (in->atk) {
                    f->combo = (f->combo_win > 0 && f->combo < 4) ? f->combo + 1 : 1;
                    f->combo_win = 0; f->queued = 0; f->state = FS_ATTACK; f->timer = 0;
                } else if (in->jutsu > 0) {
                    start_jutsu(f, in->jutsu - 1);
                } else if (in->dash && f->chakra >= 10.0f) {
                    float m2 = sqrtf(mx * mx + mz * mz);
                    f->chakra -= 10.0f;
                    if (m2 > 0.3f) { f->dashx = mx / m2; f->dashz = mz / m2; }
                    else { f->dashx = -f->fx; f->dashz = -f->fz; }
                    f->state = FS_DASH; f->timer = 0; f->inv = 14;
                }
            } break;
            case FS_ATTACK:
                f->timer++;
                if (f->timer < 6 && dist > 1.6f) { f->x += f->fx * 0.08f; f->z += f->fz * 0.08f; }
                if (f->timer == 5) melee_hit(f, o);
                if (in->atk && f->timer >= 4 && f->combo < 4) f->queued = 1;
                if (f->timer >= (f->combo == 4 ? 18 : 12)) {
                    if (f->queued) { f->queued = 0; f->combo++; f->timer = 0; }
                    else { f->state = FS_IDLE; f->combo_win = 20; }
                }
                break;
            case FS_JUTSU:
                f->timer++;
                jutsu_tick(f, o, &d->jut[f->move]);
                break;
            case FS_CHARGE:
                f->timer++;
                if (try_special(f, in)) break;
                f->chakra = fminf(100.0f, f->chakra + 0.4f);
                if (!in->charge) f->state = FS_IDLE;
                break;
            case FS_DASH:
                f->timer++;
                f->x += f->dashx * 0.45f; f->z += f->dashz * 0.45f;
                if (f->timer >= 8) f->state = FS_IDLE;
                break;
            case FS_ULT:
                f->timer++;
                if (!o->dead) o->frozen = 1;
                if (f->timer == 1) { f->chakra = 0.0f; o->inv = 0; }
                if ((f->def == 4 || f->def == 5) && f->timer >= 40 && f->timer < 60 && dist > 1.4f) {
                    f->x += f->fx * 0.5f; f->z += f->fz * 0.5f;
                }
                if (f->timer == 60) deal_damage(o, f, (float)d->ult_dmg, 30, 0.6f);
                if (f->timer >= 100) { o->frozen = 0; f->state = FS_IDLE; }
                break;
            case FS_AWAKEN:
                f->timer++;
                if (f->timer >= 40) f->state = FS_IDLE;
                break;
            }
        }
    }

    f->y += f->vy; f->vy -= 0.012f;
    if (f->y < 0) { f->y = 0; f->vy = 0; }
    r = sqrtf(f->x * f->x + f->z * f->z);
    if (r > ARENA_R) { f->x *= ARENA_R / r; f->z *= ARENA_R / r; }
}

/* ---------------- AI ---------------- */
static int ai_move_ok(const Move *m, float dist, float hpr)
{
    switch (m->type) {
    case MV_DASH:    return dist > 2.5f && dist < 8.0f;
    case MV_MULTI:   return dist > 2.0f && dist < 9.0f;
    case MV_PROJ: case MV_SPREAD: case MV_HOMING: case MV_DOT: case MV_BARRAGE:
                     return dist > 3.5f;
    case MV_GROUND:  return dist > 1.5f;
    case MV_SLASH:   return dist < 4.2f;
    case MV_PUSH:    return dist < m->range - 0.8f;
    case MV_WARP:    return dist > 3.0f && dist < 10.0f;
    case MV_HEAL:    return hpr < 0.55f;
    }
    return 0;
}

static void ai_think(Fighter *f, Fighter *o, Input *in)
{
    const Def *d = &roster[f->def];
    float dx = o->x - f->x, dz = o->z - f->z, dist = sqrtf(dx * dx + dz * dz);
    memset(in, 0, sizeof(*in));
    if (o->dead) return;
    if (f->ai_cd > 0) f->ai_cd--;
    if (--f->ai_mode_t <= 0) { f->ai_mode = rand() % 4; f->ai_mode_t = 25 + rand() % 40; }
    if (f->ai_guard > 0) { f->ai_guard--; in->guard = 1; return; }
    if (f->ai_charge > 0) { f->ai_charge--; in->charge = 1; return; }
    if (dist > 2.1f) in->ay = 1.0f; else if (dist < 1.4f) in->ay = -0.5f;
    if (f->ai_mode == 1) in->ax = 0.8f;
    else if (f->ai_mode == 2) in->ax = -0.8f;
    else if (f->ai_mode == 3 && dist < 4.0f) in->ay = -0.6f;
    if (o->state == FS_ATTACK && dist < 3.0f && rand() % 100 < 6) f->ai_guard = 18 + rand() % 14;
    if (dist <= 2.3f && f->ai_cd == 0 && f->state == FS_IDLE) { in->atk = 1; f->ai_cd = 14 + rand() % 20; }
    if (f->state == FS_ATTACK && dist < 2.5f && rand() % 100 < 30) in->atk = 1;

    if (f->state == FS_IDLE && rand() % 100 < 5) {
        int k = rand() % 3;
        const Move *m = &d->jut[k];
        if (f->chakra >= (float)m->cost && f->jcd[k] == 0 && ai_move_ok(m, dist, f->hp / f->maxhp))
            in->jutsu = k + 1;
    }
    if (f->chakra >= 100.0f && dist < 12.0f && rand() % 100 < 6) in->ult = 1;
    if (f->awm >= 100.0f && d->has_awk && f->awk == 0 && rand() % 100 < 10) in->awaken = 1;
    if (f->chakra < 30.0f && dist > 5.0f && rand() % 100 < 8) f->ai_charge = 40;
}

/* ---------------- input ---------------- */
static float deadzone(float v) { return fabsf(v) < 0.25f ? 0.0f : v; }

static Input read_input(const SceCtrlData *p, unsigned pressed)
{
    Input in;
    memset(&in, 0, sizeof(in));
    in.ax = deadzone(((float)p->Lx - 128.0f) / 128.0f);
    in.ay = deadzone(-((float)p->Ly - 128.0f) / 128.0f);
    if (p->Buttons & PSP_CTRL_LEFT)  in.ax = -1.0f;
    if (p->Buttons & PSP_CTRL_RIGHT) in.ax = 1.0f;
    if (p->Buttons & PSP_CTRL_UP)    in.ay = 1.0f;
    if (p->Buttons & PSP_CTRL_DOWN)  in.ay = -1.0f;
    in.atk = !!(pressed & PSP_CTRL_CIRCLE);
    if (pressed & PSP_CTRL_SQUARE) {
        in.jutsu = 1;
        if (p->Buttons & PSP_CTRL_UP) in.jutsu = 2;
        else if (p->Buttons & PSP_CTRL_DOWN) in.jutsu = 3;
    }
    in.jump = !!(pressed & PSP_CTRL_CROSS);
    in.dash = !!(pressed & PSP_CTRL_RTRIGGER);
    in.charge = !!(p->Buttons & PSP_CTRL_TRIANGLE);
    in.guard = !!(p->Buttons & PSP_CTRL_LTRIGGER);
    if ((p->Buttons & PSP_CTRL_TRIANGLE) && (pressed & PSP_CTRL_CIRCLE)) { in.ult = 1; in.atk = 0; }
    if ((p->Buttons & PSP_CTRL_LTRIGGER) && (p->Buttons & PSP_CTRL_TRIANGLE) && (pressed & PSP_CTRL_RTRIGGER)) {
        in.awaken = 1; in.dash = 0;
    }
    return in;
}

/* ---------------- camera ---------------- */
static Fighter *ult_caster(void)
{
    if (pl.state == FS_ULT) return &pl;
    if (en.state == FS_ULT) return &en;
    return NULL;
}

static void camera_update(int snap)
{
    float ex, ey, ez, cx, cy, cz, k = snap ? 1.0f : 0.15f;
    Fighter *c = ult_caster();

    if (c) {
        Fighter *o = (c == &pl) ? &en : &pl;
        float rx = -c->fz, rz = c->fx;
        k = 0.25f;
        if (c->timer < 60) {
            ex = c->x + rx * 3.0f + c->fx * 1.5f; ey = 1.9f; ez = c->z + rz * 3.0f + c->fz * 1.5f;
            cx = c->x; cy = 1.5f; cz = c->z;
        } else {
            ex = o->x - c->fx * 2.0f + rx * 4.0f; ey = 2.2f; ez = o->z - c->fz * 2.0f + rz * 4.0f;
            cx = o->x; cy = 1.3f; cz = o->z;
        }
    } else {
        float dx = en.x - pl.x, dz = en.z - pl.z, dist = sqrtf(dx * dx + dz * dz), fx, fz, back;
        if (dist < 0.01f) dist = 0.01f;
        fx = dx / dist; fz = dz / dist;
        back = 5.0f + dist * 0.35f;
        ex = pl.x - fx * back; ey = 2.8f + dist * 0.12f; ez = pl.z - fz * back;
        cx = pl.x + fx * dist * 0.55f; cy = 1.2f; cz = pl.z + fz * dist * 0.55f;
    }
    camx += (ex - camx) * k; camy += (ey - camy) * k; camz += (ez - camz) * k;
    tgx += (cx - tgx) * k;   tgy += (cy - tgy) * k;   tgz += (cz - tgz) * k;
}

/* ---------------- battle control ---------------- */
static void battle_start(int pdef, int edef, int story)
{
    memset(projs, 0, sizeof(projs));
    make_fighter(&pl, pdef, -4.0f, 0.0f, 0);
    make_fighter(&en, edef, 4.0f, 0.0f, 1);
    story_mode = story;
    intro = 0; ko_timer = 0; paused = 0; msg_t = 0;
    srand(frame * 2654435761u + 7u);
    camera_update(1);
    gstate = GS_BATTLE;
}

static void battle_update(const SceCtrlData *pad, unsigned pressed)
{
    Input pin, ein;
    int i;
    float dx, dz, dist;

    if (pressed & PSP_CTRL_START) paused = !paused;
    if (paused) {
        if (pressed & PSP_CTRL_SELECT) { paused = 0; gstate = GS_MENU; }
        return;
    }
    if (msg_t > 0) msg_t--;
    intro++;
    if (intro < 90) { camera_update(0); return; }

    pin = read_input(pad, pressed);
    ai_think(&en, &pl, &ein);
    if (pl.dead) memset(&pin, 0, sizeof(pin));
    fighter_update(&pl, &en, &pin);
    fighter_update(&en, &pl, &ein);
    if (pl.dead) en.frozen = 0;
    if (en.dead) pl.frozen = 0;

    dx = en.x - pl.x; dz = en.z - pl.z; dist = sqrtf(dx * dx + dz * dz);
    if (dist < 1.0f && dist > 0.001f) {
        float push = (1.0f - dist) * 0.5f;
        pl.x -= dx / dist * push; pl.z -= dz / dist * push;
        en.x += dx / dist * push; en.z += dz / dist * push;
    }

    for (i = 0; i < MAXP; i++) {
        Proj *p = &projs[i];
        float px, pz;
        if (!p->active) continue;
        if (p->homing) {
            float tx = p->tgt->x - p->x, tz = p->tgt->z - p->z, tl = sqrtf(tx * tx + tz * tz);
            if (tl > 0.01f) {
                float nvx = p->vx * 0.92f + tx / tl * p->speed * 0.08f;
                float nvz = p->vz * 0.92f + tz / tl * p->speed * 0.08f;
                float nl = sqrtf(nvx * nvx + nvz * nvz);
                if (nl > 0.001f) { p->vx = nvx / nl * p->speed; p->vz = nvz / nl * p->speed; }
            }
        }
        p->x += p->vx; p->z += p->vz;
        if (--p->life <= 0 || sqrtf(p->x * p->x + p->z * p->z) > ARENA_R + 3.0f) { p->active = 0; continue; }
        px = p->tgt->x - p->x; pz = p->tgt->z - p->z;
        {
            float rad = 0.5f + p->size * 0.6f;
            if (px * px + pz * pz < rad * rad && fabsf(p->tgt->y + 1.0f - p->y) < 1.5f) {
                int res = deal_damage(p->tgt, p->src, p->dmg, p->stun, p->kb);
                if (res != 0) {
                    p->active = 0;
                    if (p->dot && res == 1) { p->tgt->burn = 180; p->tgt->burn_dps = p->dotdmg / 180.0f; }
                }
            }
        }
    }

    if (story_mode && !en.dead) {
        float r = en.hp / en.maxhp;
        if (r <= 0.6f && en.phase_lvl < 1) { en.phase_lvl = 1; en.dmg_mult = 1.25f; en.spd_mult = 1.1f; set_msg("MADARA: SUSANOO!"); }
        if (r <= 0.3f && en.phase_lvl < 2) { en.phase_lvl = 2; en.dmg_mult = 1.5f; en.spd_mult = 1.2f; set_msg("MADARA: SIX PATHS POWER!"); }
    }

    camera_update(0);

    if (pl.dead || en.dead) {
        if (++ko_timer > 110) {
            player_won = en.dead && !pl.dead;
            result_sel = 0;
            gstate = GS_RESULT;
        }
    }
}

/* ---------------- rendering ---------------- */
static void part(const Fighter *f, float rxo, float yo, float fzo, float w, float h, float dd, unsigned col)
{
    float rx = -f->fz, rz = f->fx;
    gfx_box_rot(f->x + rx * rxo + f->fx * fzo, f->y + yo, f->z + rz * rxo + f->fz * fzo,
                w, h, dd, atan2f(f->fx, f->fz), col);
}

static void draw_fighter(const Fighter *f)
{
    const Def *d = &roster[f->def];
    unsigned body = d->body, acc = d->accent, hair = d->hair, skin = RGB(240, 200, 160);
    float sw = sinf(f->phase) * 0.35f;
    int i;
    const Move *mv = (f->state == FS_JUTSU) ? &d->jut[f->move] : NULL;

    if (f->hit_flash > 0) body = RGB(255, 255, 255);
    gfx_box(f->x, 0.03f, f->z, 1.1f, 0.02f, 1.1f, RGB(20, 20, 20));

    if (f->dead) {
        gfx_box_rot(f->x, 0.25f, f->z, 0.9f, 0.5f, 1.9f, atan2f(f->fx, f->fz), body);
        part(f, 0.0f, 0.3f, 1.2f, 0.45f, 0.45f, 0.45f, skin);
        return;
    }
    if (f->phase_lvl >= 1) part(f, 0.0f, 1.9f, -0.7f, 2.4f, 3.6f, 0.3f, RGB(90, 110, 255));
    if (f->phase_lvl >= 2) part(f, 0.0f, 4.0f, -0.7f, 1.4f, 0.3f, 0.3f, RGB(255, 255, 255));

    /* awakening aura */
    if (f->awk > 0) {
        for (i = 0; i < 4; i++) {
            float a = frame * 0.1f + i * 1.57f, h = 1.6f + 0.5f * sinf(frame * 0.3f + i);
            gfx_box(f->x + cosf(a) * 0.95f, h * 0.5f, f->z + sinf(a) * 0.95f, 0.15f, h, 0.15f, d->awk_col);
        }
        if (f->def == 1) part(f, 0.0f, 1.9f, -0.7f, 2.4f, 3.6f, 0.3f, d->awk_col);
        if (f->def == 3) part(f, 0.0f, 3.4f, -0.5f, 1.6f, 0.3f, 0.3f, d->awk_col);
        if (f->def == 2) part(f, 0.0f, 1.95f, 0.24f, 0.5f, 0.1f, 0.05f, d->awk_col);
    }
    if (f->burn > 0) {
        for (i = 0; i < 3; i++)
            gfx_box(f->x + sinf(frame * 0.7f + i * 2.0f) * 0.35f, 0.6f + ((frame + i * 3) % 9) * 0.2f,
                    f->z + cosf(frame * 0.5f + i * 2.0f) * 0.35f, 0.3f, 0.3f, 0.3f, RGB(45, 0, 70));
    }

    part(f, -0.22f, 0.45f, sw, 0.30f, 0.9f, 0.30f, acc);
    part(f,  0.22f, 0.45f, -sw, 0.30f, 0.9f, 0.30f, acc);
    part(f, 0.0f, 1.3f, 0.0f, 0.8f, 0.8f, 0.4f, body);
    part(f, 0.0f, 1.95f, 0.0f, 0.45f, 0.45f, 0.45f, skin);
    part(f, 0.0f, 2.22f, 0.0f, 0.5f, 0.15f, 0.5f, hair);

    if ((f->state == FS_ATTACK && f->timer >= 3 && f->timer <= 9) ||
        (mv && (mv->type == MV_DASH || mv->type == MV_SLASH) && f->timer >= 4 && f->timer <= 16)) {
        part(f, -0.5f, 1.4f, 0.7f, 0.22f, 0.22f, 1.1f, skin);
        part(f,  0.5f, 1.2f, 0.1f, 0.2f, 0.7f, 0.2f, skin);
    } else if (f->guarding) {
        part(f, -0.15f, 1.5f, 0.45f, 0.5f, 0.2f, 0.2f, skin);
        part(f,  0.15f, 1.35f, 0.45f, 0.5f, 0.2f, 0.2f, skin);
    } else {
        part(f, -0.55f, 1.3f, -sw * 0.5f, 0.2f, 0.7f, 0.2f, skin);
        part(f,  0.55f, 1.3f,  sw * 0.5f, 0.2f, 0.7f, 0.2f, skin);
    }

    if (mv) {
        int t = f->timer;
        switch (mv->type) {
        case MV_DASH:
            part(f, -0.5f, 1.4f, 1.5f, 0.5f, 0.5f, 0.5f, d->orb);
            break;
        case MV_SLASH:
            if (t >= 6 && t <= 14) part(f, 0.0f, 1.6f, 2.2f, 0.4f, 2.6f, 3.4f, d->orb);
            break;
        case MV_PUSH:
            if (t >= 10) {
                float s = (t - 10) * mv->range * 0.35f;
                gfx_box(f->x, 0.12f, f->z, s, 0.1f, s, d->orb);
            }
            break;
        case MV_GROUND:
            if (t >= 8 && t < 30 && ((t / 3) & 1)) gfx_box(f->mark_x, 0.07f, f->mark_z, 2.2f, 0.04f, 2.2f, RGB(255, 60, 60));
            if (t >= 30 && t < 37) {
                float h = 4.5f * (t - 30) / 6.0f;
                gfx_box(f->mark_x, h * 0.5f, f->mark_z, 1.6f, h, 1.6f, d->orb);
            }
            break;
        case MV_HEAL:
            part(f, 0.0f, 2.9f + 0.2f * sinf(t * 0.4f), 0.0f, 0.4f, 0.4f, 0.4f, RGB(120, 255, 150));
            break;
        case MV_MULTI:
            if (t >= 6 && t <= 30) {
                Fighter c = *f;
                c.x += (-f->fz) * 1.4f; c.z += f->fx * 1.4f;
                c.state = FS_ATTACK; c.timer = 5; c.hit_flash = 0; c.awk = 0; c.burn = 0;
                draw_fighter(&c);
                c.x = f->x - (-f->fz) * 1.4f; c.z = f->z - f->fx * 1.4f;
                draw_fighter(&c);
            }
            break;
        case MV_WARP:
            if (t >= 3 && t <= 8) part(f, 0.0f, 1.5f, 0.0f, 1.4f, 2.4f, 1.4f, RGB(160, 80, 220));
            break;
        default:
            if (t < 14) {
                float s = 0.2f + t * 0.04f * (mv->size + 0.5f);
                part(f, 0.0f, 1.5f, 0.9f, s, s, s, d->orb);
            }
            break;
        }
    }
    if (f->state == FS_AWAKEN) {
        gfx_box(f->x, 1.5f, f->z, 0.6f + f->timer * 0.04f, 3.0f, 0.6f + f->timer * 0.04f, d->awk_col);
    }
    if (f->state == FS_CHARGE) {
        gfx_box(f->x, 0.05f, f->z, 1.8f, 0.04f, 1.8f, RGB(80, 160, 255));
        for (i = 0; i < 4; i++) {
            float a = f->timer * 0.2f + i * 1.57f, yy = fmodf(f->timer * 0.04f + i * 0.5f, 2.2f);
            gfx_box(f->x + cosf(a) * 0.8f, yy, f->z + sinf(a) * 0.8f, 0.12f, 0.12f, 0.12f, RGB(120, 200, 255));
        }
    }
}

/* big visuals for each Ultimate Jutsu */
static void draw_ult_fx(const Fighter *c, const Fighter *o)
{
    const Def *d = &roster[c->def];
    int t = c->timer;
    float dist = dist_xz(c->x, c->z, o->x, o->z);
    float s;

    switch (c->def) {
    case 0: /* Tailed Beast Bomb: growing dark orb that flies at the target */
        s = 0.4f + t * 0.04f; if (s > 3.0f) s = 3.0f;
        if (t < 40) part(c, 0.0f, 2.8f, 0.5f, s, s, s, RGB(70, 30, 120));
        else if (t < 60) {
            float k = (t - 40) / 20.0f;
            gfx_box(c->x + (o->x - c->x) * k, 2.4f - k * 1.2f, c->z + (o->z - c->z) * k, s, s, s, RGB(70, 30, 120));
        }
        break;
    case 1: /* Indra's Arrow: bright beam */
    case 4: /* Kamui lightning */
        if (t >= 30 && t < 60) part(c, 0.0f, 1.6f, 0.9f, 0.25f + (t - 30) * 0.02f, 0.25f, 0.8f + (t - 30) * 0.05f, d->orb);
        if (t >= 60 && t < 72) gfx_box_rot(c->x + (o->x - c->x) * 0.5f, 1.4f, c->z + (o->z - c->z) * 0.5f,
                                           0.5f, 0.5f, dist, atan2f(c->fx, c->fz), d->orb);
        break;
    case 2: /* Great Buddha: giant statue rises behind, fist strikes */
        s = t * 0.18f; if (s > 8.0f) s = 8.0f;
        part(c, 0.0f, s * 0.5f, -2.0f, 4.0f, s, 2.5f, RGB(110, 190, 110));
        if (t >= 40 && t < 64) {
            float k = (t - 40) / 20.0f; if (k > 1.0f) k = 1.0f;
            gfx_box(c->x + (o->x - c->x) * k, 2.5f, c->z + (o->z - c->z) * k, 3.0f, 3.0f, 3.0f, RGB(130, 210, 130));
        }
        break;
    case 3: /* Meteor Drop */
        if (t < 20) part(c, 0.0f, 3.0f, 0.5f, 0.8f, 0.8f, 0.8f, d->orb);
        else if (t < 62) {
            float y = 18.0f - (t - 20) * 0.4f; if (y < 1.8f) y = 1.8f;
            gfx_box(o->x, y, o->z, 4.0f, 4.0f, 4.0f, d->orb);
        }
        break;
    default: /* Sakura: charged fist glow */
        s = 0.3f + t * 0.02f;
        part(c, 0.0f, 1.5f, 0.9f, s, s, s, RGB(255, 120, 190));
        break;
    }
    if (t >= 60 && t < 74) {
        float e = (t - 60) * 0.45f + 1.0f;
        gfx_box(o->x, 1.2f, o->z, e, e, e, RGB(255, 255, 255));
    }
}

static void draw_arena(int style)
{
    int i, j;
    unsigned a = style ? RGB(150, 130, 100) : RGB(95, 120, 95);
    unsigned b = style ? RGB(125, 108, 84) : RGB(80, 102, 80);
    gfx_box(0, -0.2f, 0, 26.0f, 0.4f, 26.0f, RGB(50, 50, 55));
    for (i = -3; i <= 3; i++)
        for (j = -3; j <= 3; j++)
            gfx_box(i * 3.2f, 0.01f, j * 3.2f, 3.1f, 0.02f, 3.1f, ((i + j) & 1) ? a : b);
    for (i = 0; i < 16; i++) {
        float ang = i * 0.3927f, h = 2.0f + (i % 3) * 1.2f;
        gfx_box(cosf(ang) * 11.8f, h * 0.5f, sinf(ang) * 11.8f, 0.9f, h, 0.9f, style ? RGB(110, 100, 90) : RGB(120, 120, 130));
    }
}

static void draw_bar(float x, float y, float w, float h, float ratio, unsigned col, int rtl)
{
    float fw;
    if (ratio < 0) ratio = 0;
    if (ratio > 1) ratio = 1;
    fw = w * ratio;
    gfx_rect(x - 1, y - 1, w + 2, h + 2, RGB(0, 0, 0));
    gfx_rect(x, y, w, h, RGB(55, 55, 55));
    if (rtl) gfx_rect(x + w - fw, y, fw, h, col); else gfx_rect(x, y, fw, h, col);
}

static unsigned hp_color(float r) { return r > 0.5f ? RGB(60, 220, 80) : (r > 0.25f ? RGB(240, 210, 40) : RGB(230, 50, 50)); }

static void draw_hud(void)
{
    char buf[48];
    int k;
    float x;
    const Def *pd = &roster[pl.def];
    const char *en_name = roster[en.def].name;
    Fighter *c = ult_caster();
    static const char *pre[3] = { "SQ:", "UP+SQ:", "DN+SQ:" };

    gfx_2d();
    gfx_text(10, 6, 2, RGB(255, 255, 255), pd->name);
    draw_bar(10, 24, 190, 10, pl.hp / pl.maxhp, hp_color(pl.hp / pl.maxhp), 0);
    draw_bar(10, 38, 130, 6, pl.chakra / 100.0f, RGB(70, 150, 255), 0);
    draw_bar(10, 48, 100, 4, pl.awk > 0 ? (float)pl.awk / AWAKEN_FRAMES : pl.awm / 100.0f,
             pl.awk > 0 ? pd->awk_col : RGB(255, 200, 40), 0);
    gfx_text(470 - gfx_text_width(en_name, 2), 6, 2, RGB(255, 255, 255), en_name);
    draw_bar(280, 24, 190, 10, en.hp / en.maxhp, hp_color(en.hp / en.maxhp), 1);
    draw_bar(340, 38, 130, 6, en.chakra / 100.0f, RGB(70, 150, 255), 1);
    draw_bar(370, 48, 100, 4, en.awk > 0 ? (float)en.awk / AWAKEN_FRAMES : en.awm / 100.0f,
             en.awk > 0 ? roster[en.def].awk_col : RGB(255, 200, 40), 1);
    if (pl.combo > 1) { sprintf(buf, "%d HIT", pl.combo); gfx_text(10, 62, 2, RGB(255, 220, 60), buf); }
    if (msg_t > 0) gfx_text(240 - gfx_text_width(msg, 2) / 2, 80, 2, RGB(255, 200, 60), msg);
    if (intro < 60) gfx_text(240 - gfx_text_width("READY", 4) / 2, 110, 4, RGB(255, 255, 255), "READY");
    else if (intro < 90) gfx_text(240 - gfx_text_width("FIGHT!", 4) / 2, 110, 4, RGB(255, 80, 60), "FIGHT!");
    if (pl.dead || en.dead) {
        const char *t = en.dead ? "VICTORY" : "DEFEATED";
        gfx_text(240 - gfx_text_width(t, 4) / 2, 110, 4, RGB(255, 220, 60), t);
    }

    /* jutsu readiness */
    x = 10;
    for (k = 0; k < 3; k++) {
        const Move *m = &pd->jut[k];
        int ready = (pl.chakra >= (float)m->cost && pl.jcd[k] == 0);
        sprintf(buf, "%s%s", pre[k], m->name);
        gfx_text(x, 234, 1, ready ? RGB(255, 255, 255) : RGB(120, 120, 120), buf);
        x += gfx_text_width(buf, 1) + 10;
    }
    if (pl.chakra >= 100.0f) gfx_text(10, 246, 1, RGB(255, 220, 60), "ULTIMATE READY: HOLD TRIANGLE + CIRCLE");
    else if (pl.awm >= 100.0f && pd->has_awk && pl.awk == 0) gfx_text(10, 246, 1, RGB(255, 200, 40), "AWAKENING READY: L + R + TRIANGLE");
    gfx_text(10, 262, 1, RGB(200, 200, 200), "O:ATK  X:JUMP  L:GUARD  R:DODGE  TRI:CHARGE  START:PAUSE");

    /* ultimate cinematic */
    if (c) {
        gfx_rect(0, 0, 480, 26, RGB(0, 0, 0));
        gfx_rect(0, 246, 480, 26, RGB(0, 0, 0));
        if (c->timer < 58) {
            const char *n = roster[c->def].ult_name;
            gfx_text(240 - gfx_text_width(n, 2) / 2, 8, 2, RGB(255, 220, 60), n);
        }
        if (c->timer >= 60 && c->timer < 68) {
            int a = 255 - (c->timer - 60) * 30;
            if (a < 0) a = 0;
            gfx_rect(0, 0, 480, 272, RGBA(255, 255, 255, a));
        }
    }
    if (paused) {
        gfx_rect(100, 90, 280, 80, RGBA(0, 0, 0, 220));
        gfx_text(240 - gfx_text_width("PAUSED", 3) / 2, 100, 3, RGB(255, 255, 255), "PAUSED");
        gfx_text(120, 140, 1, RGB(220, 220, 220), "START: RESUME   SELECT: QUIT TO MENU");
    }
}

static void draw_battle(void)
{
    int i;
    Fighter *c = ult_caster();
    gfx_3d();
    gfx_camera(camx, camy, camz, tgx, tgy, tgz);
    draw_arena(story_mode);
    draw_fighter(&pl);
    draw_fighter(&en);
    if (c) draw_ult_fx(c, (c == &pl) ? &en : &pl);
    for (i = 0; i < MAXP; i++)
        if (projs[i].active) {
            Proj *p = &projs[i];
            gfx_box(p->x, p->y, p->z, p->size + 0.2f, p->size + 0.2f, p->size + 0.2f, p->col);
            gfx_box(p->x - p->vx * 2.0f, p->y, p->z - p->vz * 2.0f, p->size * 0.6f, p->size * 0.6f, p->size * 0.6f, p->col);
        }
    draw_hud();
}

static void draw_preview(int def, float ang, float x)
{
    Fighter pv;
    memset(&pv, 0, sizeof(pv));
    pv.def = def; pv.x = x; pv.fx = sinf(ang); pv.fz = cosf(ang); pv.phase = ang * 2.0f;
    draw_fighter(&pv);
}

/* ---------------- main ---------------- */
int main(void)
{
    SceCtrlData pad;
    unsigned prev = 0, pressed;
    static const char *items[3] = { "STORY MODE", "FREE BATTLE", "EXIT" };
    int i;

    setup_callbacks();
    gfx_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;
        frame++;
        menu_angle += 0.02f;

        /* ---- update ---- */
        switch (gstate) {
        case GS_MENU:
            if (pressed & PSP_CTRL_UP)   menu_sel = (menu_sel + 2) % 3;
            if (pressed & PSP_CTRL_DOWN) menu_sel = (menu_sel + 1) % 3;
            if (pressed & PSP_CTRL_CROSS) {
                if (menu_sel == 2) sceKernelExitGame();
                else if (menu_sel == 0) { story_mode = 1; dlg_idx = 0; gstate = GS_DIALOGUE; }
                else { story_mode = 0; gstate = GS_SELECT; }
            }
            break;
        case GS_SELECT:
            if (pressed & PSP_CTRL_LEFT)  select_idx = (select_idx + NUM_CHARS - 1) % NUM_CHARS;
            if (pressed & PSP_CTRL_RIGHT) select_idx = (select_idx + 1) % NUM_CHARS;
            if (pressed & PSP_CTRL_CIRCLE) gstate = GS_MENU;
            if (pressed & PSP_CTRL_CROSS) {
                int e;
                srand(frame);
                e = rand() % NUM_CHARS;
                if (e == select_idx) e = (e + 1) % NUM_CHARS;
                battle_start(select_idx, e, 0);
            }
            break;
        case GS_DIALOGUE:
            if (pressed & PSP_CTRL_CROSS) {
                if (++dlg_idx >= DLG_COUNT) battle_start(DEF_HASHIRAMA, DEF_MADARA, 1);
            }
            if (pressed & PSP_CTRL_CIRCLE) gstate = GS_MENU;
            break;
        case GS_BATTLE:
            battle_update(&pad, pressed);
            break;
        case GS_RESULT:
            if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN)) result_sel ^= 1;
            if (pressed & PSP_CTRL_CROSS) {
                if (result_sel == 0) {
                    if (story_mode) battle_start(DEF_HASHIRAMA, DEF_MADARA, 1);
                    else battle_start(pl.def, en.def, 0);
                } else gstate = GS_MENU;
            }
            break;
        }

        /* ---- render ---- */
        switch (gstate) {
        case GS_MENU:
            gfx_begin(RGB(15, 20, 50));
            gfx_3d();
            gfx_camera(0.0f, 2.2f, 6.0f, 0.0f, 1.3f, 0.0f);
            gfx_box(3.5f, -0.05f, 0, 5.0f, 0.1f, 5.0f, RGB(60, 70, 90));
            draw_preview((int)(frame / 120) % NUM_CHARS, menu_angle, 3.5f);
            gfx_2d();
            gfx_text(16, 24, 4, RGB(255, 160, 30), "NARUTO STORM 4");
            gfx_text(18, 64, 2, RGB(255, 255, 255), "PPSSPP EDITION");
            for (i = 0; i < 3; i++) {
                char buf[32];
                sprintf(buf, "%s%s", i == menu_sel ? "> " : "  ", items[i]);
                gfx_text(30, 120.0f + i * 30, 3, i == menu_sel ? RGB(255, 220, 60) : RGB(200, 200, 200), buf);
            }
            gfx_text(10, 255, 1, RGB(160, 160, 160), "UP/DOWN: SELECT   CROSS: CONFIRM   PROTOTYPE - PLACEHOLDER MODELS");
            gfx_end();
            break;
        case GS_SELECT: {
            char buf[64];
            const Def *sd = &roster[select_idx];
            gfx_begin(RGB(25, 30, 60));
            gfx_3d();
            gfx_camera(0.0f, 2.0f, 5.5f, 0.0f, 1.2f, 0.0f);
            gfx_box(0, -0.05f, 0, 5.0f, 0.1f, 5.0f, RGB(70, 80, 100));
            draw_preview(select_idx, menu_angle, 0.0f);
            gfx_2d();
            gfx_text(240 - gfx_text_width("SELECT FIGHTER", 2) / 2, 8, 2, RGB(255, 255, 255), "SELECT FIGHTER");
            gfx_text(20, 110, 4, RGB(255, 220, 60), "<");
            gfx_text(436, 110, 4, RGB(255, 220, 60), ">");
            gfx_text(240 - gfx_text_width(sd->name, 3) / 2, 150, 3, RGB(255, 255, 255), sd->name);
            sprintf(buf, "HP %d", sd->hp);
            gfx_text(240 - gfx_text_width(buf, 1) / 2, 178, 1, RGB(200, 200, 200), buf);
            for (i = 0; i < 3; i++) {
                sprintf(buf, "JUTSU %d: %s (%d CHAKRA)", i + 1, sd->jut[i].name, sd->jut[i].cost);
                gfx_text(240 - gfx_text_width(buf, 1) / 2, 192.0f + i * 11, 1, RGB(120, 200, 255), buf);
            }
            sprintf(buf, "ULTIMATE: %s", sd->ult_name);
            gfx_text(240 - gfx_text_width(buf, 1) / 2, 228, 1, RGB(255, 220, 60), buf);
            sprintf(buf, "AWAKENING: %s", sd->has_awk ? sd->awk_name : "NONE YET");
            gfx_text(240 - gfx_text_width(buf, 1) / 2, 240, 1, RGB(255, 160, 80), buf);
            gfx_text(10, 262, 1, RGB(160, 160, 160), "LEFT/RIGHT: CHOOSE   CROSS: FIGHT   CIRCLE: BACK");
            gfx_end();
        } break;
        case GS_DIALOGUE:
            gfx_begin(RGB(30, 20, 20));
            gfx_2d();
            gfx_text(16, 14, 3, RGB(255, 160, 30), "CHAPTER 1");
            gfx_text(16, 46, 2, RGB(255, 255, 255), "HASHIRAMA VS MADARA");
            gfx_rect(10, 130, 460, 130, RGBA(0, 0, 0, 230));
            gfx_text(20, 138, 2, RGB(255, 220, 60), dlg_spk[dlg_idx]);
            gfx_text_wrap(20, 164, 2, RGB(255, 255, 255), dlg_txt[dlg_idx], 36);
            gfx_text(330, 246, 1, RGB(180, 180, 180), "CROSS: NEXT  CIRCLE: BACK");
            gfx_end();
            break;
        case GS_BATTLE:
            gfx_begin(story_mode ? RGB(200, 120, 80) : RGB(110, 170, 235));
            draw_battle();
            gfx_end();
            break;
        case GS_RESULT:
            gfx_begin(RGB(15, 15, 25));
            gfx_2d();
            {
                const char *t = player_won ? "MISSION COMPLETE" : "MISSION FAILED";
                gfx_text(240 - gfx_text_width(t, 3) / 2, 70, 3, player_won ? RGB(255, 220, 60) : RGB(230, 70, 70), t);
                gfx_text(150, 150, 2, result_sel == 0 ? RGB(255, 220, 60) : RGB(200, 200, 200), result_sel == 0 ? "> RETRY" : "  RETRY");
                gfx_text(150, 180, 2, result_sel == 1 ? RGB(255, 220, 60) : RGB(200, 200, 200), result_sel == 1 ? "> MAIN MENU" : "  MAIN MENU");
            }
            gfx_end();
            break;
        }
    }
    return 0;
}
