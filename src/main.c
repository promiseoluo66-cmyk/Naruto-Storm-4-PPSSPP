/*
 * Naruto Storm 4 PPSSPP - Phase 1 prototype
 * Menu -> Story dialogue -> 1v1 battle with AI, chakra, ninjutsu, guard.
 * Rendering is PSP debug-screen text for now (placeholder, not 3D yet).
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("NarutoStorm4", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define W 60
#define H 14
#define pr pspDebugScreenPrintf

typedef enum { ST_MENU, ST_DIALOGUE, ST_BATTLE, ST_RESULT } State;

typedef struct {
    int x, y, hp, chakra, cd;
    char sym;
    const char *name;
} Fighter;

static State state = ST_MENU;
static int menu_sel = 0;
static int story_mode = 0;
static int dlg_idx = 0;
static int player_won = 0;
static unsigned frame = 0;
static Fighter pl, en;
static char msg[64] = "";
static int msg_t = 0;

static const char *dialogue[] = {
    "Narrator: Two legends of the shinobi world clash once more.",
    "Madara: Hashirama... I'll show you my power.",
    "Hashirama: Then I'll end this here, Madara."
};
#define DLG_COUNT 3

/* ---- exit callback ---- */
static int exit_cb(int a1, int a2, void *c) { sceKernelExitGame(); return 0; }
static int cb_thread(SceSize args, void *argp) {
    int id = sceKernelCreateCallback("Exit", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
}

static int iabs(int v) { return v < 0 ? -v : v; }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void say(const char *s) { strncpy(msg, s, 63); msg[63] = 0; msg_t = 60; }

static void start_battle(void) {
    pl = (Fighter){ 10, H / 2, 100, 50, 0, story_mode ? 'H' : 'N', story_mode ? "Hashirama" : "Naruto" };
    en = (Fighter){ W - 10, H / 2, 100, 50, 0, story_mode ? 'M' : 'S', story_mode ? "Madara" : "Sasuke" };
    msg[0] = 0;
    state = ST_BATTLE;
}

static void hit(Fighter *a, Fighter *b, int dmg, int range, int guard) {
    if (iabs(a->x - b->x) <= range && iabs(a->y - b->y) <= 1) {
        if (guard) dmg /= 2;
        b->hp -= dmg;
        if (b->hp < 0) b->hp = 0;
        b->x = clampi(b->x + (b->x >= a->x ? 2 : -2), 1, W - 2);
    }
}

static void bar(const char *label, int hp, int chakra) {
    char b[24]; int i, n = hp / 5, c = chakra / 10;
    for (i = 0; i < 20; i++) b[i] = i < n ? '#' : '-';
    b[20] = 0;
    pr("%-10s HP[%s] %3d  CHK ", label, b, hp);
    for (i = 0; i < 10; i++) pr("%c", i < c ? '*' : '.');
    pr("\n");
}

static void draw_battle(void) {
    static char g[H][W + 1];
    int x, y;
    pspDebugScreenSetXY(0, 0);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) g[y][x] = (y == H - 1) ? '=' : ' ';
        g[y][W] = 0;
    }
    g[pl.y][pl.x] = pl.sym;
    g[en.y][en.x] = en.sym;
    pr("%s\n", story_mode ? "STORY - Ch1: Hashirama vs. Madara" : "FREE BATTLE");
    bar(pl.name, pl.hp, pl.chakra);
    bar(en.name, en.hp, en.chakra);
    for (y = 0; y < H; y++) pr("%s\n", g[y]);
    pr("%-62s\n", msg_t > 0 ? msg : "");
    pr("Pad/Stick:Move  O:Attack  []:Ninjutsu(25)  /\\:Charge  L:Guard\n");
}

static void update_battle(SceCtrlData *pad, unsigned pressed) {
    int dx = 0, dy = 0;
    if (msg_t > 0) msg_t--;
    if ((pad->Buttons & PSP_CTRL_LEFT) || pad->Lx < 64) dx = -1;
    if ((pad->Buttons & PSP_CTRL_RIGHT) || pad->Lx > 192) dx = 1;
    if ((pad->Buttons & PSP_CTRL_UP) || pad->Ly < 64) dy = -1;
    if ((pad->Buttons & PSP_CTRL_DOWN) || pad->Ly > 192) dy = 1;
    if (frame % 2 == 0) {
        pl.x = clampi(pl.x + dx, 1, W - 2);
        pl.y = clampi(pl.y + dy, 0, H - 2);
    }
    if (pl.cd > 0) pl.cd--;
    if (en.cd > 0) en.cd--;

    if ((pressed & PSP_CTRL_CIRCLE) && pl.cd == 0) {
        pl.cd = 12; hit(&pl, &en, 8, 3, 0); say("Attack!");
    }
    if ((pressed & PSP_CTRL_SQUARE) && pl.cd == 0 && pl.chakra >= 25) {
        pl.chakra -= 25; pl.cd = 30;
        hit(&pl, &en, 30, 6, 0);
        say(story_mode ? "Wood Style: Wood Dragon!" : "Rasengan!");
    }
    if ((pad->Buttons & PSP_CTRL_TRIANGLE) && frame % 3 == 0) pl.chakra = clampi(pl.chakra + 1, 0, 100);
    if (frame % 40 == 0) { pl.chakra = clampi(pl.chakra + 1, 0, 100); en.chakra = clampi(en.chakra + 2, 0, 100); }

    /* simple AI */
    if (frame % 3 == 0) {
        if (iabs(en.x - pl.x) > 3) en.x += (pl.x < en.x) ? -1 : 1;
        if (en.y != pl.y) en.y += (pl.y < en.y) ? -1 : 1;
        if (iabs(en.x - pl.x) <= 3 && en.cd == 0 && rand() % 4 == 0) {
            en.cd = 20;
            if (en.chakra >= 25 && rand() % 3 == 0) {
                en.chakra -= 25;
                hit(&en, &pl, 24, 6, pad->Buttons & PSP_CTRL_LTRIGGER);
                say(story_mode ? "Madara: Fire Style!" : "Sasuke: Chidori!");
            } else {
                hit(&en, &pl, 7, 3, pad->Buttons & PSP_CTRL_LTRIGGER);
            }
        }
    }
    if (en.hp <= 0) { player_won = 1; state = ST_RESULT; pspDebugScreenClear(); }
    else if (pl.hp <= 0) { player_won = 0; state = ST_RESULT; pspDebugScreenClear(); }
}

static void draw_menu(void) {
    const char *items[] = { "Story Mode", "Free Battle", "Exit" };
    int i;
    pspDebugScreenSetXY(0, 0);
    pr("==== NARUTO STORM 4 - PPSSPP EDITION (Phase 1) ====\n\n");
    for (i = 0; i < 3; i++) pr(" %c %-20s\n", i == menu_sel ? '>' : ' ', items[i]);
    pr("\nUp/Down: select   X: confirm\n");
    pr("\nPrototype: text-based placeholder graphics.\n");
}

int main(void) {
    SceCtrlData pad;
    unsigned prev = 0, pressed;
    setup_callbacks();
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pspDebugScreenClear();

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;
        frame++;

        switch (state) {
        case ST_MENU:
            draw_menu();
            if (pressed & PSP_CTRL_UP) menu_sel = (menu_sel + 2) % 3;
            if (pressed & PSP_CTRL_DOWN) menu_sel = (menu_sel + 1) % 3;
            if (pressed & PSP_CTRL_CROSS) {
                pspDebugScreenClear();
                if (menu_sel == 2) sceKernelExitGame();
                story_mode = (menu_sel == 0);
                if (story_mode) { dlg_idx = 0; state = ST_DIALOGUE; }
                else start_battle();
            }
            break;
        case ST_DIALOGUE:
            pspDebugScreenSetXY(0, 0);
            pr("CHAPTER 1: Hashirama vs. Madara\n\n");
            pr("%s\n\nX: next\n", dialogue[dlg_idx]);
            if (pressed & PSP_CTRL_CROSS) {
                pspDebugScreenClear();
                if (++dlg_idx >= DLG_COUNT) start_battle();
            }
            break;
        case ST_BATTLE:
            update_battle(&pad, pressed);
            if (state == ST_BATTLE) draw_battle();
            break;
        case ST_RESULT:
            pspDebugScreenSetXY(0, 0);
            pr("%s\n\n", player_won ? "MISSION COMPLETE!" : "MISSION FAILED");
            pr("X: back to menu\n");
            if (pressed & PSP_CTRL_CROSS) { pspDebugScreenClear(); state = ST_MENU; }
            break;
        }
        sceDisplayWaitVblankStart();
    }
    return 0;
}
