/*
 * launcher.c - real entry point. Shows the model test viewer first, then hands
 * over to the game. main.c is compiled with -Dmain=game_main (see Makefile),
 * so the existing game code is untouched.
 */
#include <pspkernel.h>
#include <pspgu.h>

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
    int t = sceKernelCreateThread("cb_launch", cb_thread, 0x11, 0xFA0, 0, 0);
    if (t >= 0) sceKernelStartThread(t, 0, 0);

    model_test_run();
    sceGuTerm(); /* game_main() initialises the GU again */
    return game_main();
}
