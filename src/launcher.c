/*
 * Direct Free Battle launcher.
 * The game starts immediately in the Naruto-vs-Naruto training scene.
 * PPSSPP supplies the on-screen PSP controls; this program draws none.
 */
extern int game_main(void);

int main(void)
{
    return game_main();
}
