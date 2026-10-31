/*
 * Launcher for the two test targets:
 * CROSS = enter the playable combat prototype (menus, free battle and story).
 * TRIANGLE = open the imported character model viewer.
 * No virtual PSP controls are drawn; PPSSPP supplies the controller overlay.
 */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include "gfx.h"

extern int game_main(void);
extern int model_test_run(void);

static int exit_cb(int a1, int a2, void *c)
{
    sceKernelExitGame();
    return 0;
}

static int cb_thread(SceSize args, void *argp)
{
    int id = sceKernelCreateCallback("ExitLauncher", exit_cb, NULL);
    sceKernelRegisterExitCallback(id);
    sceKernelSleepThreadCB();
    return 0;
}

int main(void)
{
    SceCtrlData pad;
    unsigned prev = 0, pressed;
    int selection = 0;
    int t = sceKernelCreateThread("cb_launch", cb_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);
    gfx_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;
        if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN)) selection ^= 1;
        if (pressed & PSP_CTRL_CIRCLE) selection = 0;
        if (pressed & PSP_CTRL_CROSS) {
            if (selection == 0) game_main();
            else model_test_run();
            sceKernelExitGame();
            return 0;
        }
        if (pressed & PSP_CTRL_TRIANGLE) {
            model_test_run();
            sceKernelExitGame();
            return 0;
        }

        gfx_begin(RGB(8, 12, 32));
        gfx_2d();
        gfx_gradient(0, 0, 480, 272, RGB(8, 14, 42), RGB(48, 22, 62));
        gfx_rect(18, 18, 444, 236, RGBA(4, 7, 20, 210));
        gfx_rect(18, 18, 4, 236, RGB(255, 145, 35));
        gfx_text(38, 42, 3, RGB(255, 155, 35), "NARUTO STORM");
        gfx_text(38, 78, 2, RGB(255, 225, 115), "PROTOTYPE TEST LAUNCHER");
        if (selection == 0) {
            gfx_rect(34, 122, 410, 34, RGBA(255, 145, 35, 220));
            gfx_text(48, 132, 2, RGB(15, 18, 35), "COMBAT / ANIMATION TEST");
            gfx_text(48, 170, 2, RGB(220, 230, 250), "CHARACTER MODEL VIEWER");
        } else {
            gfx_text(48, 132, 2, RGB(220, 230, 250), "COMBAT / ANIMATION TEST");
            gfx_rect(34, 160, 410, 34, RGBA(255, 145, 35, 220));
            gfx_text(48, 170, 2, RGB(15, 18, 35), "CHARACTER MODEL VIEWER");
        }
        gfx_text(38, 220, 1, RGB(190, 205, 230), "UP/DOWN: SELECT   CROSS: OPEN   TRIANGLE: VIEWER");
        gfx_text(38, 238, 1, RGB(155, 175, 210), "No in-game controller artwork; use PPSSPP controls");
        gfx_end();
        sceDisplayWaitVblankStart();
    }
    return 0;
}
