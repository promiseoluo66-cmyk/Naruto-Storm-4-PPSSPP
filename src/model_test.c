/*
 * model_test.c - PSP-era character model viewer.
 * Keeps the viewer as the boot screen; the unfinished game menu stays disabled.
 * Reports mesh and texture diagnostics without drawing fake on-screen PSP controls.
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
        gfx_end();
    }

    model_free(&m);
    model_texture_free(&tex);
    return 0;
}
