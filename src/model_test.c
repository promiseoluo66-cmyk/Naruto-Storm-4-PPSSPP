/*
 * model_test.c - PSP-era character model viewer.
 * Keeps the viewer as the boot screen; the unfinished game menu stays disabled.
 * The HUD borrows the layout of a classic PSP debug/model viewer while reporting
 * the REAL triangle count and texture dimensions of whichever asset loaded.
 */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "gfx.h"
#include "model.h"

extern const unsigned char test_character_nmdl[];
extern const unsigned int test_character_nmdl_size;
extern const unsigned char test_character_nmtx[];
extern const unsigned int test_character_nmtx_size;

#define IMPORT_MODEL_FILE "assets/chr/test/naruto_import.nmdl"
#define IMPORT_TEX_FILE   "assets/chr/test/naruto_import.nmtx"
#define MODEL_FILE "assets/chr/test/test_character.nmdl"
#define TEX_FILE   "assets/chr/test/test_texture.nmtx"

static void outline_box(float x, float y, float w, float h,
                        float thickness, unsigned color)
{
    gfx_rect(x, y, w, thickness, color);
    gfx_rect(x, y + h - thickness, w, thickness, color);
    gfx_rect(x, y, thickness, h, color);
    gfx_rect(x + w - thickness, y, thickness, h, color);
}

/* Small scanline circle keeps the virtual PSP buttons crisp at 480x272. */
static void circle_outline(float cx, float cy, int radius, unsigned color)
{
    int y;
    for (y = -radius; y <= radius; y++) {
        float q = (float)(radius * radius - y * y);
        int x = (int)sqrtf(q);
        gfx_rect(cx - x, cy + y, 2, 1, color);
        gfx_rect(cx + x - 1, cy + y, 2, 1, color);
    }
}

static void draw_dpad(void)
{
    unsigned edge = RGB(225, 230, 245);
    unsigned face = RGBA(35, 42, 68, 220);
    float x = 35.0f, y = 112.0f;

    gfx_rect(x + 9, y, 12, 32, face);
    gfx_rect(x, y + 10, 30, 12, face);
    outline_box(x + 9, y, 12, 32, 1, edge);
    outline_box(x, y + 10, 30, 12, 1, edge);
    gfx_rect(x + 11, y + 12, 8, 8, RGB(105, 115, 145));
    gfx_text(x + 12, y - 7, 1, edge, "U");
    gfx_text(x - 7, y + 13, 1, edge, "L");
    gfx_text(x + 27, y + 13, 1, edge, "R");
    gfx_text(x + 12, y + 33, 1, edge, "D");
}

static void draw_analog(void)
{
    unsigned edge = RGB(210, 220, 240);
    circle_outline(50, 204, 17, edge);
    circle_outline(50, 204, 11, RGBA(145, 165, 205, 180));
    gfx_rect(46, 200, 9, 9, RGBA(45, 55, 85, 235));
    outline_box(46, 200, 9, 9, 1, edge);
    gfx_text(28, 228, 1, edge, "ANALOG");
}

static void draw_face_buttons(void)
{
    unsigned text = RGB(245, 245, 250);
    circle_outline(426, 106, 10, RGB(100, 205, 125));
    gfx_text(423, 103, 1, text, "T");
    circle_outline(447, 127, 10, RGB(230, 100, 105));
    gfx_text(444, 124, 1, text, "O");
    circle_outline(405, 127, 10, RGB(105, 165, 235));
    gfx_text(402, 124, 1, text, "X");
    outline_box(416, 139, 19, 19, 2, RGB(220, 130, 185));
    gfx_text(422, 145, 1, text, "S");
}

static void draw_r_button(void)
{
    outline_box(450, 7, 21, 19, 1, RGB(245, 245, 250));
    gfx_text(457, 12, 1, RGB(245, 245, 250), "R");
}

int model_test_run(void)
{
    Model m;
    ModelTexture tex;
    const char *msrc = "IMPORTED", *tsrc = "IMPORTED";
    int mr, tr, use_tex = 1, spin = 1;
    float yaw = 0.0f;
    unsigned prev = 0, pressed;
    SceCtrlData pad;
    char line[64];

    gfx_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    mr = model_load(&m, IMPORT_MODEL_FILE);
    if (mr < 0) {
        msrc = "FILE";
        mr = model_load(&m, MODEL_FILE);
    }
    if (mr < 0) {
        msrc = "EMBEDDED";
        mr = model_load_memory(&m, test_character_nmdl, test_character_nmdl_size);
    }
    tr = model_texture_load(&tex, IMPORT_TEX_FILE);
    if (tr < 0) {
        tsrc = "FILE";
        tr = model_texture_load(&tex, TEX_FILE);
    }
    if (tr < 0) {
        tsrc = "EMBEDDED";
        tr = model_texture_load_memory(&tex, test_character_nmtx, test_character_nmtx_size);
    }

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;

        /* PSP controls: D-pad or analog to rotate; Cross resets the view. */
        if (pressed & PSP_CTRL_CROSS) yaw = 0.0f;
        if (pressed & (PSP_CTRL_SQUARE | PSP_CTRL_SELECT | PSP_CTRL_START))
            spin = !spin;
        if (pressed & PSP_CTRL_TRIANGLE) use_tex = !use_tex;
        if (pad.Buttons & PSP_CTRL_LEFT)  yaw -= 0.05f;
        if (pad.Buttons & PSP_CTRL_RIGHT) yaw += 0.05f;
        if (pad.Lx < 108 || pad.Lx > 148)
            yaw += ((float)pad.Lx - 128.0f) * 0.0007f;
        if (spin) yaw += 0.015f;

        gfx_begin(RGB(22, 27, 62));
        gfx_2d();
        gfx_gradient(0, 0, 480, 272, RGB(21, 31, 78), RGB(78, 48, 96));
        gfx_rect(0, 57, 480, 1, RGBA(180, 190, 230, 100));
        gfx_rect(0, 228, 480, 1, RGBA(180, 190, 230, 100));

        gfx_3d();
        gfx_camera(0.0f, 1.05f, 2.85f, 0.0f, 0.83f, 0.0f);
        /* A low, flat display plinth beneath the character. */
        gfx_box(0.0f, -0.055f, 0.0f, 2.75f, 0.10f, 2.15f, RGB(65, 73, 105));
        gfx_box(0.0f, -0.112f, 0.0f, 2.95f, 0.035f, 2.32f, RGB(35, 42, 75));
        if (mr == 0) {
            if (use_tex && tr == 0) model_draw_textured(&m, &tex, 0.0f, 0.0f, 0.0f, yaw);
            else model_draw(&m, 0.0f, 0.0f, 0.0f, yaw);
        }

        gfx_2d();
        /* Header and live model diagnostics. */
        gfx_text(8, 7, 2, RGB(255, 220, 65), "CHARACTER MODEL VIEWER - 1.66M TARGET");
        if (mr == 0) {
            sprintf(line, "MESH: %s  %u TRIANGLES", msrc, m.vertex_count / 3);
            gfx_text(10, 31, 1, RGB(220, 232, 255), line);
        } else {
            sprintf(line, "MODEL LOAD FAILED: CODE %d", mr);
            gfx_text(10, 31, 1, RGB(255, 115, 115), line);
        }
        if (tr == 0) {
            sprintf(line, "TEXTURE: %s %ux%u %s", tsrc, tex.width, tex.height,
                    (use_tex ? "ON" : "OFF"));
            gfx_text(10, 43, 1, RGB(220, 232, 255), line);
        } else {
            sprintf(line, "TEXTURE LOAD FAILED: CODE %d", tr);
            gfx_text(10, 43, 1, RGB(255, 115, 115), line);
        }
        draw_r_button();

        /* PSP-style virtual control overlay. */
        draw_dpad();
        draw_analog();
        draw_face_buttons();

        gfx_text(10, 237, 1, RGB(238, 240, 250), "LEFT/RIGHT: ROTATE");
        gfx_text(10, 249, 1, RGB(238, 240, 250), "CROSS: RESET VIEW");
        gfx_text(247, 237, 1, RGB(238, 240, 250), "SQUARE / SELECT / START:");
        gfx_text(247, 249, 1, RGB(255, 220, 90), spin ? "AUTO ROTATE: ON" : "AUTO ROTATE: OFF");
        gfx_text(160, 262, 1, RGB(245, 205, 100), "(GAME MENU DISABLED)");
        gfx_end();
    }

    model_free(&m);
    model_texture_free(&tex);
    return 0;
}
