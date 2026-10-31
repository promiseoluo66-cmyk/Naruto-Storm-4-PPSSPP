/*
 * launcher.c - PSP executable entry point.
 *
 * Start the actual game flow in main.c (compiled as game_main), not the
 * standalone character model viewer. The viewer remains available as a
 * development tool but must not replace the game's menu at startup.
 */
extern int game_main(void);

int main(void)
{
    return game_main();
}
