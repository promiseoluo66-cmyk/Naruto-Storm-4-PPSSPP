/*
 * Naruto Storm 4 PPSSPP - Phase 2
 * 3D arena combat: lock-on camera, combos, ninjutsu projectiles, guard, dodge,
 * chakra, AI opponent, health bars, win/lose. Story ch1 mission + free battle.
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
#define MAXP 8

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
typedef struct {
    const char *name, *nin;
    int hp;
    float speed;
    int nin_dmg;
    unsigned body, accent, hair, orb;
} Def;

#define NUM_CHARS 6
static const Def roster[NUM_CHARS] = {
    { "NARUTO",    "RASENGAN",        1000, 0.135f, 120, RGB(245,140,20),  RGB(25,50,190),  RGB(250,215,60), RGB(80,180,255)  },
    { "SASUKE",    "CHIDORI",          950, 0.145f, 125, RGB(235,235,235), RGB(40,50,120),  RGB(25,25,35),   RGB(140,170,255) },
    { "HASHIRAMA", "WOOD DRAGON",     1150, 0.120f, 150, RGB(150,40,40),   RGB(60,60,60),   RGB(30,25,20),   RGB(60,200,80)   },
    { "MADARA",    "FIRE STYLE",      1100, 0.125f, 110, RGB(120,30,40),   RGB(70,70,80),   RGB(20,20,25),   RGB(255,110,30)  },
    { "KAKASHI",   "LIGHTNING BLADE", 900,  0.150f, 130, RGB(50,70,110),   RGB(50,60,70),   RGB(190,190,200), RGB(160,200,255) },
    { "SAKURA",    "CHAKRA PUNCH",    850,  0.145f, 130, RGB(210,70,90),   RGB(240,240,240), RGB(255,150,190), RGB(255,200,230) }
};
#define DEF_HASHIRAMA 2
#define DEF_MADARA 3

/* ---------------- fighter ---------------- */
typedef enum { FS_IDLE, FS_ATTACK, FS_NIN, FS_CHARGE, FS_DASH } FState;

typedef struct {
    int def, is_ai;
    float x, y, z, vy, fx, fz;
    float hp, maxhp, chakra, dmg_mult, spd_mult;
    FState state;
    int timer, combo, combo_win, queued, stun, inv, nin_cd, guarding, dead, hit_flash, phase_lvl;
    float kbx, kbz, dashx, dashz, phase;
    int ai_cd, ai_mode, ai_mode_t, ai_guard, ai_charge;
} Fighter;

typedef struct {
    float ax, ay;
    int atk, nin, charge, guard, jump, dash;
} Input;

typedef struct {
    int active, life, dmg;
    float x, y, z, vx, vz;
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
    if (t->guarding && stun < 20) {
        t->hp -= dmg * 0.2f;
        t->x += dx / len * 0.3f; t->z += dz / len * 0.3f;
        if (t->hp <= 0) { t->hp = 0; t->dead = 1; }
        return 2;
    }
    t->hp -= dmg;
    if (t->hp <= 0) { t->hp = 0; t->dead = 1; }
    t->stun = stun; t->kbx = dx / len * kb; t->kbz = dz / len * kb;
    t->state = FS_IDLE; t->combo = 0; t->queued = 0; t->hit_flash = 6;
    return 1;
}

static void melee_hit(Fighter *f, Fighter *o)
{
    static const float dmg[5] = { 0, 18, 18, 22, 40 };
    static const int stn[5] = { 0, 8, 8, 10, 22 };
    static const float kb[5] = { 0, 0.06f, 0.06f, 0.08f, 0.35f };
    float dx = o->x - f->x, dz = o->z - f->z;
    int c = f->combo;
    if (c < 1 || c > 4) return;
    if (sqrtf(dx * dx + dz * dz) <= 2.4f && fabsf(f->y - o->y) < 1.6f) {
        if (deal_damage(o, f, dmg[c], stn[c], kb[c]) == 1) f->chakra = fminf(100.0f, f->chakra + 4.0f);
    }
}

static void spawn_proj(Fighter *f, Fighter *o)
{
    int i;
    for (i = 0; i < MAXP; i++) {
        if (!projs[i].active) {
            Proj *p = &projs[i];
            p->active = 1; p->life = 100;
            p->x = f->x + f->fx; p->y = f->y + 1.3f; p->z = f->z + f->fz;
            p->vx = f->fx * 0.32f; p->vz = f->fz * 0.32f;
            p->src = f; p->tgt = o;
            p->col = roster[f->def].orb;
            p->dmg = roster[f->def].nin_dmg;
            return;
        }
    }
}

static void fighter_update(Fighter *f, Fighter *o, const Input *in)
{
    const Def *d = &roster[f->def];
    float dx = o->x - f->x, dz = o->z - f->z, dist = sqrtf(dx * dx + dz * dz), rx, rz, r;

    if (dist < 0.01f) dist = 0.01f;
    f->fx = dx / dist; f->fz = dz / dist;
    rx = -f->fz; rz = f->fx;
    f->guarding = 0;
    if (f->dead) { f->y = 0; return; }
    if (f->inv > 0) f->inv--;
    if (f->nin_cd > 0) f->nin_cd--;
    if (f->hit_flash > 0) f->hit_flash--;
    if (f->combo_win > 0 && --f->combo_win == 0) f->combo = 0;
    f->chakra = fminf(100.0f, f->chakra + 0.05f);

    if (f->stun > 0) {
        f->stun--;
        f->x += f->kbx; f->z += f->kbz;
        f->kbx *= 0.8f; f->kbz *= 0.8f;
        f->state = FS_IDLE;
    } else {
        switch (f->state) {
        case FS_IDLE: {
            float mx = rx * in->ax + f->fx * in->ay, mz = rz * in->ax + f->fz * in->ay;
            float ml = sqrtf(mx * mx + mz * mz), sp = d->speed * f->spd_mult;
            if (ml > 1.0f) { mx /= ml; mz /= ml; ml = 1.0f; }
            if (in->guard) { f->guarding = 1; sp *= 0.4f; }
            f->x += mx * sp; f->z += mz * sp;
            f->phase += ml * 0.35f;
            if (in->jump && f->y <= 0.001f) f->vy = 0.24f;
            if (in->charge && !in->guard) { f->state = FS_CHARGE; f->timer = 0; }
            else if (in->atk) {
                f->combo = (f->combo_win > 0 && f->combo < 4) ? f->combo + 1 : 1;
                f->combo_win = 0; f->queued = 0; f->state = FS_ATTACK; f->timer = 0;
            } else if (in->nin && f->chakra >= 25.0f && f->nin_cd == 0) {
                f->chakra -= 25.0f; f->nin_cd = 60; f->state = FS_NIN; f->timer = 0;
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
        case FS_NIN:
            f->timer++;
            if (f->timer == 10) spawn_proj(f, o);
            if (f->timer >= 26) f->state = FS_IDLE;
            break;
        case FS_CHARGE:
            f->timer++;
            f->chakra = fminf(100.0f, f->chakra + 0.7f);
            if (!in->charge) f->state = FS_IDLE;
            break;
        case FS_DASH:
            f->timer++;
            f->x += f->dashx * 0.45f; f->z += f->dashz * 0.45f;
            if (f->timer >= 8) f->state = FS_IDLE;
            break;
        }
    }

    f->y += f->vy; f->vy -= 0.012f;
    if (f->y < 0) { f->y = 0; f->vy = 0; }
    r = sqrtf(f->x * f->x + f->z * f->z);
    if (r > ARENA_R) { f->x *= ARENA_R / r; f->z *= ARENA_R / r; }
}

/* ---------------- AI ---------------- */
static void ai_think(Fighter *f, Fighter *o, Input *in)
{
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
    if (dist > 4.0f && f->chakra >= 25.0f && f->nin_cd == 0 && rand() % 100 < 3) in->nin = 1;
    if (f->chakra < 20.0f && dist > 5.0f && rand() % 100 < 8) f->ai_charge = 40;
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
    in.nin = !!(pressed & PSP_CTRL_SQUARE);
    in.jump = !!(pressed & PSP_CTRL_CROSS);
    in.dash = !!(pressed & PSP_CTRL_RTRIGGER);
    in.charge = !!(p->Buttons & PSP_CTRL_TRIANGLE);
    in.guard = !!(p->Buttons & PSP_CTRL_LTRIGGER);
    return in;
}

/* ---------------- camera ---------------- */
static void camera_targets(const Fighter *p, const Fighter *e, float *ex, float *ey, float *ez, float *cx, float *cy, float *cz)
{
    float dx = e->x - p->x, dz = e->z - p->z, dist = sqrtf(dx * dx + dz * dz), fx, fz, back;
    if (dist < 0.01f) dist = 0.01f;
    fx = dx / dist; fz = dz / dist;
    back = 5.0f + dist * 0.35f;
    *ex = p->x - fx * back; *ey = 2.8f + dist * 0.12f; *ez = p->z - fz * back;
    *cx = p->x + fx * dist * 0.55f; *cy = 1.2f; *cz = p->z + fz * dist * 0.55f;
}

static void camera_update(int snap)
{
    float ex, ey, ez, cx, cy, cz, k = snap ? 1.0f : 0.15f;
    camera_targets(&pl, &en, &ex, &ey, &ez, &cx, &cy, &cz);
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
        p->x += p->vx; p->z += p->vz;
        if (--p->life <= 0 || sqrtf(p->x * p->x + p->z * p->z) > ARENA_R + 3.0f) { p->active = 0; continue; }
        px = p->tgt->x - p->x; pz = p->tgt->z - p->z;
        if (px * px + pz * pz < 0.8f * 0.8f && fabsf(p->tgt->y + 1.0f - p->y) < 1.5f) {
            if (deal_damage(p->tgt, p->src, (float)p->dmg, 16, 0.25f) != 0) p->active = 0;
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

    if (f->hit_flash > 0) body = RGB(255, 255, 255);
    gfx_box(f->x, 0.03f, f->z, 1.1f, 0.02f, 1.1f, RGB(20, 20, 20));

    if (f->dead) {
        gfx_box_rot(f->x, 0.25f, f->z, 0.9f, 0.5f, 1.9f, atan2f(f->fx, f->fz), body);
        part(f, 0.0f, 0.3f, 1.2f, 0.45f, 0.45f, 0.45f, skin);
        return;
    }
    if (f->phase_lvl >= 1) part(f, 0.0f, 1.9f, -0.7f, 2.4f, 3.6f, 0.3f, RGB(90, 110, 255));
    if (f->phase_lvl >= 2) part(f, 0.0f, 4.0f, -0.7f, 1.4f, 0.3f, 0.3f, RGB(255, 255, 255));

    part(f, -0.22f, 0.45f, sw, 0.30f, 0.9f, 0.30f, acc);
    part(f,  0.22f, 0.45f, -sw, 0.30f, 0.9f, 0.30f, acc);
    part(f, 0.0f, 1.3f, 0.0f, 0.8f, 0.8f, 0.4f, body);
    part(f, 0.0f, 1.95f, 0.0f, 0.45f, 0.45f, 0.45f, skin);
    part(f, 0.0f, 2.22f, 0.0f, 0.5f, 0.15f, 0.5f, hair);

    if (f->state == FS_ATTACK && f->timer >= 3 && f->timer <= 9) {
        part(f, -0.5f, 1.4f, 0.7f, 0.22f, 0.22f, 1.1f, skin);
        part(f,  0.5f, 1.2f, 0.1f, 0.2f, 0.7f, 0.2f, skin);
    } else if (f->guarding) {
        part(f, -0.15f, 1.5f, 0.45f, 0.5f, 0.2f, 0.2f, skin);
        part(f,  0.15f, 1.35f, 0.45f, 0.5f, 0.2f, 0.2f, skin);
    } else {
        part(f, -0.55f, 1.3f, -sw * 0.5f, 0.2f, 0.7f, 0.2f, skin);
        part(f,  0.55f, 1.3f,  sw * 0.5f, 0.2f, 0.7f, 0.2f, skin);
    }
    if (f->state == FS_NIN && f->timer < 14)
        part(f, 0.0f, 1.5f, 0.9f, 0.3f + f->timer * 0.03f, 0.3f + f->timer * 0.03f, 0.3f + f->timer * 0.03f, d->orb);
    if (f->state == FS_CHARGE) {
        gfx_box(f->x, 0.05f, f->z, 1.8f, 0.04f, 1.8f, RGB(80, 160, 255));
        for (i = 0; i < 4; i++) {
            float a = f->timer * 0.2f + i * 1.57f, yy = fmodf(f->timer * 0.04f + i * 0.5f, 2.2f);
            gfx_box(f->x + cosf(a) * 0.8f, yy, f->z + sinf(a) * 0.8f, 0.12f, 0.12f, 0.12f, RGB(120, 200, 255));
        }
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
    char buf[32];
    const char *en_name = roster[en.def].name;
    gfx_2d();
    gfx_text(10, 6, 2, RGB(255, 255, 255), roster[pl.def].name);
    draw_bar(10, 24, 190, 10, pl.hp / pl.maxhp, hp_color(pl.hp / pl.maxhp), 0);
    draw_bar(10, 38, 130, 6, pl.chakra / 100.0f, RGB(70, 150, 255), 0);
    gfx_text(470 - gfx_text_width(en_name, 2), 6, 2, RGB(255, 255, 255), en_name);
    draw_bar(280, 24, 190, 10, en.hp / en.maxhp, hp_color(en.hp / en.maxhp), 1);
    draw_bar(340, 38, 130, 6, en.chakra / 100.0f, RGB(70, 150, 255), 1);
    if (pl.combo > 1) { sprintf(buf, "%d HIT", pl.combo); gfx_text(10, 56, 2, RGB(255, 220, 60), buf); }
    if (msg_t > 0) gfx_text(240 - gfx_text_width(msg, 2) / 2, 80, 2, RGB(255, 200, 60), msg);
    if (intro < 60) gfx_text(240 - gfx_text_width("READY", 4) / 2, 110, 4, RGB(255, 255, 255), "READY");
    else if (intro < 90) gfx_text(240 - gfx_text_width("FIGHT!", 4) / 2, 110, 4, RGB(255, 80, 60), "FIGHT!");
    if (pl.dead || en.dead) {
        const char *t = en.dead ? "VICTORY" : "DEFEATED";
        gfx_text(240 - gfx_text_width(t, 4) / 2, 110, 4, RGB(255, 220, 60), t);
    }
    gfx_text(10, 262, 1, RGB(230, 230, 230), "CIRCLE:ATK  SQUARE:JUTSU  CROSS:JUMP  L:GUARD  R:DODGE  TRI:CHARGE");
    if (paused) {
        gfx_rect(100, 90, 280, 80, RGBA(0, 0, 0, 220));
        gfx_text(240 - gfx_text_width("PAUSED", 3) / 2, 100, 3, RGB(255, 255, 255), "PAUSED");
        gfx_text(120, 140, 1, RGB(220, 220, 220), "START: RESUME   SELECT: QUIT TO MENU");
    }
}

static void draw_battle(void)
{
    int i;
    gfx_3d();
    gfx_camera(camx, camy, camz, tgx, tgy, tgz);
    draw_arena(story_mode);
    draw_fighter(&pl);
    draw_fighter(&en);
    for (i = 0; i < MAXP; i++)
        if (projs[i].active) {
            gfx_box(projs[i].x, projs[i].y, projs[i].z, 0.55f, 0.55f, 0.55f, projs[i].col);
            gfx_box(projs[i].x - projs[i].vx * 2.0f, projs[i].y, projs[i].z - projs[i].vz * 2.0f, 0.3f, 0.3f, 0.3f, projs[i].col);
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
            char buf[48];
            gfx_begin(RGB(25, 30, 60));
            gfx_3d();
            gfx_camera(0.0f, 2.0f, 5.5f, 0.0f, 1.2f, 0.0f);
            gfx_box(0, -0.05f, 0, 5.0f, 0.1f, 5.0f, RGB(70, 80, 100));
            draw_preview(select_idx, menu_angle, 0.0f);
            gfx_2d();
            gfx_text(240 - gfx_text_width("SELECT FIGHTER", 2) / 2, 10, 2, RGB(255, 255, 255), "SELECT FIGHTER");
            gfx_text(30, 130, 4, RGB(255, 220, 60), "<");
            gfx_text(420, 130, 4, RGB(255, 220, 60), ">");
            gfx_text(240 - gfx_text_width(roster[select_idx].name, 3) / 2, 190, 3, RGB(255, 255, 255), roster[select_idx].name);
            sprintf(buf, "JUTSU: %s", roster[select_idx].nin);
            gfx_text(240 - gfx_text_width(buf, 2) / 2, 222, 2, RGB(120, 200, 255), buf);
            sprintf(buf, "HP %d", roster[select_idx].hp);
            gfx_text(240 - gfx_text_width(buf, 2) / 2, 244, 2, RGB(200, 200, 200), buf);
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
