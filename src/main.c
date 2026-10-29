/*
 * Naruto Storm 4 PPSSPP - Phase 5 + visual upgrade
 * 8 fighters (adds Obito, Kaguya), support characters (L+Square), 4 arenas,
 * boss transformation cinematics, 6 story missions (Ch1-Ch6).
 * Visual pass: detailed procedural character models, sky gradients, scenery,
 * hit sparks, screen shake.
 *
 * Models are procedural boxes (no imported assets). Move/mission data is compiled in.
 */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"

PSP_MODULE_INFO("NarutoStorm4", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define ARENA_R 9.5f
#define MAXP 12
#define MAXPART 48
#define AWAKEN_FRAMES 1800
#define NUM_ARENAS 4
#define CINE_FRAMES 75

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
    MV_GROUND, MV_SLASH, MV_PUSH, MV_WARP, MV_HEAL, MV_DOT, MV_PHASE
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

#define NUM_CHARS 100
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
      "HUNDRED SEAL STRIKE", 400, 0, "NONE", RGB(255,255,255) },

    { "OBITO", 1000, 0.140f, RGB(60,60,70), RGB(40,40,50), RGB(30,25,30), RGB(255,120,40),
      { { "FIRE STYLE",    MV_PROJ,  25, 240, 100, 14, 0.28f, 0.0f, 0.15f, 1.0f },
        { "KAMUI PHASE",   MV_PHASE, 30, 480,   0,  0, 0.00f, 0.0f, 0.00f, 0.0f },
        { "KAMUI STRIKE",  MV_WARP,  30, 420, 110, 14, 0.00f, 2.5f, 0.25f, 0.0f } },
      "TRUTH-SEEKING BARRAGE", 470, 1, "TEN-TAILS FORM", RGB(160,60,220) },

    { "KAGUYA", 1800, 0.130f, RGB(245,245,250), RGB(200,190,225), RGB(250,250,255), RGB(255,255,255),
      { { "DIMENSION SHIFT",    MV_WARP,   30, 480,  90, 14, 0.00f, 2.5f, 0.25f, 0.0f },
        { "TRUTH-SEEKING BALLS", MV_SPREAD, 40, 420, 160, 18, 0.26f, 0.0f, 0.20f, 0.6f },
        { "EXPANSIVE PUSH",     MV_PUSH,   35, 540,  70, 16, 0.00f, 6.5f, 0.70f, 0.0f } },
      "INFINITE TSUKUYOMI", 400, 0, "NONE", RGB(255,255,255) },
    { "MINATO", 930, 0.170f, RGB(235,190,65), RGB(245,240,220), RGB(245,205,80), RGB(255,225,100),
      { { "FLYING RAIJIN", MV_WARP, 30, 300, 125, 18, 0.0f, 2.5f, 0.28f, 0.0f },
        { "RASENGAN", MV_DASH, 25, 240, 120, 18, 0.48f, 2.0f, 0.30f, 0.5f },
        { "YELLOW FLASH", MV_MULTI, 35, 390, 145, 20, 0.0f, 3.2f, 0.30f, 0.0f } },
      "FLYING RAIJIN LEVEL 2", 470, 1, "KURAMA CHAKRA", RGB(255,190,55) },
    { "ITACHI", 900, 0.140f, RGB(35,30,40), RGB(150,35,45), RGB(25,20,30), RGB(170,35,65),
      { { "AMATERASU", MV_DOT, 35, 540, 115, 8, 0.27f, 0.0f, 0.0f, 0.85f },
        { "TSUKUYOMI", MV_WARP, 35, 420, 120, 22, 0.0f, 2.8f, 0.30f, 0.0f },
        { "FIRE STYLE", MV_PROJ, 25, 240, 95, 14, 0.30f, 0.0f, 0.18f, 1.1f } },
      "SUSANOO TOTSUKA BLADE", 480, 1, "SUSANOO", RGB(220,75,45) },
    { "PAIN", 1080, 0.130f, RGB(40,35,45), RGB(180,65,75), RGB(25,20,25), RGB(190,70,90),
      { { "SHINRA TENSEI", MV_PUSH, 35, 480, 105, 20, 0.0f, 6.4f, 0.7f, 0.0f },
        { "SUMMONING", MV_HOMING, 30, 360, 115, 14, 0.27f, 0.0f, 0.22f, 0.8f },
        { "CHAOTIC TENSEI", MV_GROUND, 45, 600, 155, 24, 0.0f, 3.4f, 0.48f, 0.0f } },
      "PLANETARY DEVASTATION", 490, 1, "SIX PATHS OF PAIN", RGB(155,115,190) },
    { "GAARA", 970, 0.125f, RGB(145,120,95), RGB(125,45,45), RGB(100,65,45), RGB(205,150,90),
      { { "SAND COFFIN", MV_WARP, 30, 420, 110, 18, 0.0f, 2.8f, 0.32f, 0.0f },
        { "SAND SHOWER", MV_SPREAD, 25, 300, 90, 12, 0.30f, 0.0f, 0.18f, 0.45f },
        { "SAND TSUNAMI", MV_GROUND, 40, 540, 145, 22, 0.0f, 4.2f, 0.5f, 0.0f } },
      "SAND MAUSOLEUM", 455, 1, "SHUKAKU ARMOR", RGB(190,150,100) },
    { "MIGHT GUY", 1000, 0.175f, RGB(45,135,65), RGB(35,35,40), RGB(30,35,25), RGB(80,220,100),
      { { "DYNAMIC ENTRY", MV_DASH, 25, 240, 130, 20, 0.52f, 2.2f, 0.36f, 0.5f },
        { "EVENING ELEPHANT", MV_BARRAGE, 40, 540, 150, 20, 0.35f, 0.0f, 0.28f, 0.65f },
        { "HIRUDORA", MV_PROJ, 45, 540, 175, 22, 0.28f, 0.0f, 0.3f, 1.2f } },
      "NIGHT GUY", 540, 1, "EIGHT GATES", RGB(255,70,50) },
    { "JIRAIYA", 1040, 0.135f, RGB(195,195,205), RGB(180,40,45), RGB(230,230,235), RGB(220,220,255),
      { { "RASENGAN", MV_DASH, 25, 240, 115, 18, 0.46f, 2.0f, 0.28f, 0.5f },
        { "TOAD OIL BOMB", MV_SPREAD, 35, 390, 125, 16, 0.28f, 0.0f, 0.25f, 0.7f },
        { "HAIR NEEDLES", MV_BARRAGE, 25, 360, 95, 12, 0.30f, 0.0f, 0.15f, 0.42f } },
      "SAGE ART GOEMON", 435, 1, "TOAD SAGE MODE", RGB(230,160,70) },
    { "OROCHIMARU", 950, 0.145f, RGB(220,220,220), RGB(100,70,130), RGB(40,35,50), RGB(170,130,220),
      { { "SNAKE STRIKE", MV_DASH, 25, 270, 115, 20, 0.40f, 2.0f, 0.3f, 0.5f },
        { "SNAKE SUMMON", MV_HOMING, 35, 420, 135, 16, 0.24f, 0.0f, 0.5f, 0.8f },
        { "HIDDEN SHADOW SNAKE", MV_BARRAGE, 30, 420, 120, 16, 0.28f, 0.0f, 0.22f, 0.45f } },
      "EIGHT BRANCHES", 450, 1, "WHITE SNAKE FORM", RGB(230,230,240) },
    { "KILLER BEE", 1030, 0.155f, RGB(35,35,40), RGB(30,30,35), RGB(20,20,25), RGB(90,170,245),
      { { "LARIAT", MV_DASH, 25, 240, 125, 18, 0.50f, 2.2f, 0.35f, 0.5f },
        { "INK CLONES", MV_MULTI, 25, 360, 105, 14, 0.0f, 3.0f, 0.18f, 0.0f },
        { "TAIL BEAST BOMB", MV_PROJ, 45, 540, 175, 22, 0.26f, 0.0f, 0.32f, 1.2f } },
      "EIGHT-TAILS BOMB", 475, 1, "GYUKI CHAKRA", RGB(70,140,235) },

#define EXTRA(n,hp,spd,b,a,hr,orb,m1,m2,m3,ult,ud,ha,an,ac) \
    { n, hp, spd, b, a, hr, orb, \
      { { m1, MV_DASH, 25, 240, 110, 18, 0.42f, 2.0f, 0.30f, 0.5f }, \
        { m2, MV_PROJ, 30, 330, 100, 14, 0.28f, 0.0f, 0.20f, 0.8f }, \
        { m3, MV_GROUND, 40, 480, 140, 20, 0.0f, 3.0f, 0.40f, 0.0f } }, \
      ult, ud, ha, an, ac }

    /* Added playable forms and core Leaf Village fighters */
    EXTRA("NARUTO YOUNG", 950,0.145f,RGB(235,125,25),RGB(30,55,165),RGB(255,210,55),RGB(90,175,255),"YOUNG RASENGAN","CLONE FEINT","FOX SPIRAL","ONE-TAIL RAMPAGE",390,1,"ONE-TAIL CLOAK",RGB(255,125,35)),
    EXTRA("NARUTO FOUR-TAILS", 1120,0.155f,RGB(160,45,35),RGB(80,25,25),RGB(90,30,25),RGB(255,75,35),"RED CHAKRA CLAW","TAIL SWEEP","BIJUU BLAST","FOUR-TAILS RAMPAGE",510,1,"FOUR-TAILS FORM",RGB(235,65,45)),
    EXTRA("NARUTO SIX-TAILS", 1180,0.155f,RGB(175,35,35),RGB(95,20,25),RGB(80,25,25),RGB(255,90,45),"CHAKRA ARMS","TAIL CRUSH","BIJUU SHOCKWAVE","SIX-TAILS RAGE",530,1,"SIX-TAILS FORM",RGB(255,80,50)),
    EXTRA("NARUTO KCM", 1050,0.175f,RGB(255,190,35),RGB(255,225,85),RGB(255,225,80),RGB(255,235,100),"KCM RASENGAN","CHAKRA ARM","FLASH COMBO","KURAMA AVATAR",500,1,"NINE-TAILS CHAKRA",RGB(255,205,45)),
    EXTRA("NARUTO KURAMA SAGE", 1100,0.175f,RGB(255,180,35),RGB(255,230,95),RGB(255,220,75),RGB(255,240,130),"SAGE RASENGAN","CHAKRA CLONES","SIX PATHS RASENSHURIKEN","KURAMA SAGE BOMB",540,1,"KURAMA SAGE MODE",RGB(255,215,65)),
    EXTRA("NARUTO SIX PATHS", 1120,0.180f,RGB(245,175,35),RGB(245,225,100),RGB(255,220,70),RGB(255,245,155),"LAVA RASENSHURIKEN","TRUTH SEEKER ORB","SIX PATHS BARRAGE","SIX PATHS PLANETARY RASENSHURIKEN",560,1,"SIX PATHS SAGE",RGB(255,235,125)),
    EXTRA("SASUKE YOUNG", 930,0.155f,RGB(50,55,90),RGB(240,240,245),RGB(25,25,40),RGB(130,165,255),"YOUNG CHIDORI","FIREBALL","CURSE MARK STRIKE","CURSE MARK CHIDORI",420,1,"CURSE MARK STAGE 2",RGB(145,70,210)),
    EXTRA("SASUKE TAKA", 960,0.160f,RGB(245,245,245),RGB(45,45,65),RGB(25,25,35),RGB(150,175,255),"CHIDORI STREAM","SWORD SLASH","AMATERASU","SUSANOO ARROW",450,1,"ETERNAL MANGEKYO",RGB(115,135,255)),
    EXTRA("SASUKE RINNEGAN", 1020,0.165f,RGB(90,75,100),RGB(55,40,80),RGB(20,20,30),RGB(170,140,255),"AMENOTEJIKARA","FLAME CONTROL","CHIDORI SHARP SPEAR","INDRA'S ARROW",520,1,"RINNEGAN SUSANOO",RGB(155,135,255)),
    EXTRA("SASUKE THE LAST", 970,0.160f,RGB(35,35,45),RGB(55,55,65),RGB(20,20,25),RGB(150,170,255),"CHIDORI","SWORD DRAW","BLACK FLAMES","SUSANOO FINAL SLASH",450,1,"SUSANOO ARMOR",RGB(120,145,255)),
    EXTRA("SAKURA WAR", 920,0.150f,RGB(220,75,95),RGB(245,245,245),RGB(255,155,185),RGB(255,200,225),"CHAKRA PUNCH","GROUND BREAKER","HUNDRED SEAL HEAL","SEAL RELEASE STRIKE",430,1,"STRENGTH OF A HUNDRED",RGB(255,145,180)),
    EXTRA("SAI", 880,0.150f,RGB(235,235,235),RGB(35,35,45),RGB(20,20,25),RGB(210,210,220),"INK LION","INK BIRD","SUPER BEAST SCROLL","INK BEAST ASSAULT",400,0,"NONE",RGB(255,255,255)),
    EXTRA("YAMATO", 970,0.130f,RGB(45,90,65),RGB(45,55,50),RGB(35,30,25),RGB(90,185,120),"WOOD STYLE: WALL","WOOD CLONE","WOOD DRAGON","WOOD STYLE: DEEP FOREST",430,0,"NONE",RGB(255,255,255)),
    EXTRA("SHIKAMARU", 900,0.140f,RGB(45,45,55),RGB(45,45,55),RGB(25,25,30),RGB(130,150,170),"SHADOW POSSESSION","SHADOW NECK BIND","SHADOW STRANGLE","SHADOW SEWING",390,0,"NONE",RGB(255,255,255)),
    EXTRA("INO", 850,0.150f,RGB(220,160,185),RGB(245,235,235),RGB(245,205,220),RGB(245,175,210),"MIND TRANSFER","MIND DISTURBANCE","HEALING JUTSU","MIND CLONE ASSAULT",370,0,"NONE",RGB(255,255,255)),
    EXTRA("CHOJI", 1020,0.130f,RGB(120,65,45),RGB(200,50,50),RGB(40,30,25),RGB(220,125,75),"HUMAN BULLET TANK","SPINNING FIST","PARTIAL EXPANSION","BUTTERFLY BOMB",440,1,"BUTTERFLY MODE",RGB(235,100,65)),
    EXTRA("KIBA", 940,0.165f,RGB(160,115,75),RGB(60,60,60),RGB(95,65,40),RGB(200,170,120),"FANG OVER FANG","WOLF FANG","BEAST CLONE","TWO-HEADED WOLF",410,0,"NONE",RGB(255,255,255)),
    EXTRA("SHINO", 900,0.135f,RGB(55,65,70),RGB(35,40,45),RGB(25,25,30),RGB(100,150,160),"INSECT JAR","BUG SWARM","PARASITIC BEETLES","INSECT CYCLONE",390,0,"NONE",RGB(255,255,255)),
    EXTRA("HINATA", 870,0.150f,RGB(100,105,175),RGB(210,210,225),RGB(70,65,100),RGB(170,180,255),"GENTLE FIST","EIGHT TRIGRAMS","PROTECTIVE 64 PALMS","TWIN LION FISTS",400,0,"NONE",RGB(255,255,255)),
    EXTRA("NEJI", 900,0.155f,RGB(225,225,230),RGB(170,180,195),RGB(70,65,80),RGB(185,205,255),"ROTATION","AIR PALM","64 PALMS","EIGHT TRIGRAMS PALM REVOLUTION",410,0,"NONE",RGB(255,255,255)),
    EXTRA("ROCK LEE", 980,0.180f,RGB(45,130,65),RGB(35,35,40),RGB(25,25,25),RGB(90,225,110),"LEAF HURRICANE","PRIMARY LOTUS","FRONT LOTUS","HIDDEN LOTUS",470,1,"EIGHT GATES",RGB(255,75,50)),
    EXTRA("TENTEN", 850,0.155f,RGB(165,75,70),RGB(235,220,200),RGB(45,35,30),RGB(220,150,100),"TOOL BARRAGE","DRAGON RISING","TWIN RISING DRAGONS","TREASURE TOOLS",390,0,"NONE",RGB(255,255,255)),
    EXTRA("TSUNADE", 1040,0.150f,RGB(225,190,150),RGB(65,125,85),RGB(200,175,145),RGB(120,230,150),"HEAVEN KICK","CHAKRA PUNCH","REGENERATION","CREATION REBIRTH",450,1,"HUNDRED SEAL",RGB(255,180,190)),
    EXTRA("ASUMA", 980,0.150f,RGB(65,75,65),RGB(90,90,90),RGB(45,35,30),RGB(180,145,95),"FLYING SWALLOW","BLADE RUSH","ASH PILE BURNING","ASH CLOUD ASSAULT",420,0,"NONE",RGB(255,255,255)),
    EXTRA("SHISUI", 940,0.175f,RGB(50,65,80),RGB(45,55,65),RGB(25,25,30),RGB(80,190,140),"BODY FLICKER","FIRE STYLE","KOTOAMATSUKAMI","SUSANOO DRILL",450,1,"ARMORED SUSANOO",RGB(80,200,150)),
    EXTRA("KUSHINA", 930,0.150f,RGB(185,45,55),RGB(220,220,230),RGB(200,45,60),RGB(255,100,120),"ADAMANTINE CHAINS","RED HOT HURRICANE","CHAIN BIND","CHAIN RAMPAGE",420,0,"NONE",RGB(255,255,255)),
    EXTRA("DANZO", 1010,0.145f,RGB(100,105,100),RGB(50,55,50),RGB(70,65,55),RGB(140,170,100),"WIND VACUUM","BAKU SUMMON","IZANAGI","KOTOAMATSUKAMI",450,1,"RELEASED SEAL",RGB(145,175,100)),
    EXTRA("TOBIRAMA", 1030,0.165f,RGB(225,225,230),RGB(55,75,105),RGB(220,220,230),RGB(100,170,255),"WATER DRAGON","FLYING RAIJIN","SHADOW CLONES","TACTICAL WATER BARRAGE",460,1,"REANIMATED",RGB(130,190,255)),
    EXTRA("HIRUZEN", 1000,0.145f,RGB(130,125,120),RGB(155,145,130),RGB(155,155,160),RGB(210,160,95),"FIRE DRAGON","MONKEY KING STAFF","EARTH WALL","FIVE NATURES BARRAGE",440,1,"REANIMATED",RGB(220,175,110)),
    EXTRA("TEMARI", 900,0.155f,RGB(190,175,150),RGB(70,75,85),RGB(215,190,160),RGB(175,200,225),"WIND SCYTHE","FAN GALE","SUMMONING KAMATARI","WIND STYLE NET",420,0,"NONE",RGB(255,255,255)),
    EXTRA("KANKURO", 930,0.140f,RGB(60,65,70),RGB(45,45,50),RGB(25,25,30),RGB(150,150,170),"PUPPET STRIKE","BLACK ANT TRAP","CROW BARRAGE","PUPPET SHOW",430,0,"NONE",RGB(255,255,255)),
    EXTRA("CHIYO", 880,0.125f,RGB(195,170,145),RGB(165,65,65),RGB(200,190,180),RGB(210,165,120),"PUPPET NEEDLES","PUPPET SWARM","CHAKRA SHIELD","TEN PUPPETS FORMATION",420,0,"NONE",RGB(255,255,255)),
    EXTRA("RASA", 1010,0.130f,RGB(185,165,125),RGB(110,90,70),RGB(170,145,115),RGB(210,180,115),"GOLD DUST WAVE","GOLD DUST SHIELD","DUST BURIAL","GOLD DUST MAUSOLEUM",450,1,"REANIMATED",RGB(215,185,125)),
    EXTRA("RIN NOHARA", 860,0.145f,RGB(185,195,205),RGB(90,125,155),RGB(110,115,130),RGB(150,190,230),"HEALING MIST","WATER CLONE","ISOBU CHAKRA","THREE-TAILS DEFENSE",380,0,"NONE",RGB(255,255,255)),
    EXTRA("HANABI", 870,0.155f,RGB(225,225,230),RGB(110,110,155),RGB(45,40,55),RGB(175,185,255),"GENTLE FIST","PALM STRIKE","BYAKUGAN DASH","EIGHT TRIGRAMS BARRAGE",390,0,"NONE",RGB(255,255,255)),
    EXTRA("IRUKA", 880,0.130f,RGB(95,110,115),RGB(55,65,70),RGB(45,40,35),RGB(135,160,170),"SHURIKEN THROW","SEAL BARRIER","CLONE FEINT","ACADEMY COUNTER",360,0,"NONE",RGB(255,255,255)),
    EXTRA("THIRD RAIKAGE", 1180,0.160f,RGB(65,75,85),RGB(45,50,60),RGB(190,190,195),RGB(110,185,255),"HELL STAB","LIGHTNING ARMOR","FOUR-FINGER STRIKE","ONE-FINGER HELL STAB",500,1,"REANIMATED ARMOR",RGB(100,180,255)),
    EXTRA("KISAME", 1100,0.145f,RGB(55,90,120),RGB(35,65,95),RGB(35,45,55),RGB(80,160,220),"WATER SHARK BOMB","FIVE SHARKS","SAMEHADA SLASH","WATER PRISON SHARK DANCE",470,1,"SAMEHADA FUSION",RGB(65,140,210)),
    EXTRA("DEIDARA", 900,0.155f,RGB(190,190,190),RGB(45,45,50),RGB(220,220,220),RGB(225,185,90),"CLAY BIRD","C2 DRAGON","CLAY MINES","C4 KARURA",430,0,"NONE",RGB(255,255,255)),
    EXTRA("SASORI", 920,0.140f,RGB(170,125,105),RGB(120,45,45),RGB(130,75,65),RGB(195,115,100),"IRON SAND","PUPPET NEEDLES","HIRUKO STING","RED SECRET PERFORMANCE",430,0,"NONE",RGB(255,255,255)),
    EXTRA("HIDAN", 990,0.155f,RGB(225,225,225),RGB(100,35,40),RGB(225,225,225),RGB(190,45,55),"SCYTHE SWEEP","CURSE RITUAL","TRIPLE SCYTHE","JASHIN RITUAL",450,0,"NONE",RGB(255,255,255)),
    EXTRA("KAKUZU", 1080,0.135f,RGB(45,50,55),RGB(35,40,45),RGB(25,25,30),RGB(90,140,150),"EARTH SPEAR","LIGHTNING THREAD","MASK BLAST","EARTH GRUDGE FEAR",470,1,"THREAD BODY",RGB(90,160,170)),
    EXTRA("KONAN", 890,0.155f,RGB(225,225,235),RGB(80,95,125),RGB(210,210,225),RGB(180,190,225),"PAPER SHURIKEN","PAPER SPEAR","PAPER CLONE","PAPER ANGEL",420,0,"NONE",RGB(255,255,255)),
    EXTRA("NAGATO", 1050,0.130f,RGB(170,55,55),RGB(55,45,55),RGB(100,35,35),RGB(190,115,180),"SHINRA TENSEI","SUMMONING","SOUL DRAGON","OUTER PATH DEMONIC STATUE",470,1,"REANIMATED",RGB(175,125,195)),
    EXTRA("KABUTO SAGE", 1020,0.150f,RGB(220,220,225),RGB(65,65,75),RGB(235,235,240),RGB(160,220,190),"WHITE RAGE","SNAKE STRIKE","CHAKRA SCALPEL","SAGE ART REANIMATION",470,1,"SAGE MODE",RGB(145,220,185)),
    EXTRA("KARIN", 850,0.150f,RGB(180,65,85),RGB(45,40,50),RGB(200,35,55),RGB(235,115,135),"MIND'S EYE","CHAIN BIND","HEALING BITE","ADAMANTINE CHAIN",390,0,"NONE",RGB(255,255,255)),
    EXTRA("SUIGETSU", 940,0.170f,RGB(160,195,210),RGB(60,100,125),RGB(235,235,240),RGB(115,195,225),"WATER GUN","LIQUID BODY","EXECUTIONER SWORD","WATER STYLE GREAT BUBBLE",420,1,"HYDRIFICATION",RGB(115,205,240)),
    EXTRA("JUGO", 1040,0.145f,RGB(185,145,120),RGB(65,65,70),RGB(145,110,95),RGB(195,130,110),"CURSE MARK FIST","BERSERKER RUSH","CHAKRA BLAST","SAGE TRANSFORMATION",460,1,"CURSE MARK",RGB(205,140,110)),
    EXTRA("YUGITO NII", 970,0.165f,RGB(85,110,155),RGB(35,45,65),RGB(60,75,100),RGB(105,155,220),"FIRE CLAW","CAT PAW RUSH","BLUE FLAME ORB","TWO-TAILS CHARGE",440,1,"MATATABI CLOAK",RGB(95,155,235)),
    EXTRA("YAGURA", 980,0.145f,RGB(90,150,165),RGB(40,75,85),RGB(45,60,70),RGB(100,190,205),"WATER MIRROR","CORAL PALM","WATER SHARK","THREE-TAILS ROLL",450,1,"ISOBU FORM",RGB(100,195,215)),
    EXTRA("ROSHI", 1010,0.150f,RGB(160,75,45),RGB(100,45,35),RGB(150,70,45),RGB(235,105,60),"LAVA ROCK","VOLCANIC FIST","LAVA FLOW","FOUR-TAILS ERUPTION",460,1,"SON GOKU CLOAK",RGB(240,110,65)),
    EXTRA("HAN", 1030,0.155f,RGB(165,110,80),RGB(90,60,45),RGB(150,95,70),RGB(220,145,90),"STEAM JET","BOIL RELEASE","ARMORED CHARGE","FIVE-TAILS RAM",470,1,"KOKUO FORM",RGB(225,150,95)),
    EXTRA("UTAKATA", 900,0.165f,RGB(155,190,205),RGB(60,100,130),RGB(155,185,205),RGB(125,200,235),"BUBBLE SHOT","SOAP BUBBLES","BUBBLE PRISON","SIX-TAILS BUBBLE BURST",420,1,"SAIKEN CLOAK",RGB(120,205,240)),
    EXTRA("FU", 900,0.175f,RGB(105,175,100),RGB(45,115,65),RGB(65,155,80),RGB(140,225,130),"WING DASH","SCALE POWDER","WIND BURST","SEVEN-TAILS FLIGHT",430,1,"CHOMEI FORM",RGB(135,230,145)),
    EXTRA("BORUTO", 950,0.175f,RGB(65,115,195),RGB(45,55,100),RGB(55,85,170),RGB(105,165,255),"VANISHING RASENGAN","SHADOW CLONES","LIGHTNING PALM","BORUTO STREAM",430,1,"KARMA BURST",RGB(100,170,255)),
    EXTRA("SARADA", 900,0.165f,RGB(180,55,70),RGB(35,35,45),RGB(45,35,40),RGB(225,100,125),"SHARINGAN STRIKE","FIREBALL","CHIDORI","SARADA COMBO",420,1,"SHARINGAN AWAKENING",RGB(235,105,130)),
    EXTRA("MITSUKI", 920,0.175f,RGB(175,190,200),RGB(55,95,130),RGB(205,215,225),RGB(130,220,200),"LIGHTNING SNAKE","WIND STYLE","SOFT PHYSIQUE STRIKE","SAGE LIGHTNING",440,1,"SAGE MODE",RGB(125,220,200)),
    EXTRA("MOMOSHIKI", 1120,0.160f,RGB(235,225,220),RGB(85,55,105),RGB(235,230,220),RGB(195,120,235),"RED RASENGAN","CHAKRA ABSORPTION","RINNEGAN BLAST","TENGAI SHINSEI",510,1,"FUSED MOMOSHIKI",RGB(195,115,240)),
    EXTRA("KINSHIKI", 1180,0.160f,RGB(185,80,65),RGB(95,45,40),RGB(200,150,125),RGB(235,115,85),"CHAKRA WEAPON","AXE SLASH","RED CHAKRA BURST","DIVINE WEAPON STORM",500,1,"WEAPON MANIFESTATION",RGB(235,120,90)),
    EXTRA("MIFUNE", 970,0.170f,RGB(65,70,80),RGB(50,55,65),RGB(55,50,45),RGB(175,185,200),"QUICK DRAW","IAI SLASH","CROSS CUT","SAMURAI FINAL CUT",430,0,"NONE",RGB(255,255,255)),
    EXTRA("HANZO", 1050,0.140f,RGB(100,100,105),RGB(65,65,70),RGB(80,80,85),RGB(150,155,165),"SICKLE STRIKE","POISON MIST","SALAMANDER SUMMON","POISONED SICKLE RUSH",460,1,"REANIMATED",RGB(165,165,175)),
    EXTRA("NARUTO ADULT",1050,0.175f,RGB(245,165,40),RGB(245,215,100),RGB(255,215,65),RGB(255,230,110),"ADULT RASENGAN","SHADOW CLONE FEINT","RASENSHURIKEN","KURAMA LINK BARRAGE",510,1,"ADULT KURAMA MODE",RGB(255,205,65)),
    EXTRA("NARUTO THE LAST",1020,0.175f,RGB(245,160,35),RGB(245,215,90),RGB(255,220,65),RGB(255,235,130),"SAGE RASENGAN","CHAKRA ARM SWEEP","BIJUU BOMB","SIX PATHS RASENSHURIKEN",500,1,"THE LAST KURAMA MODE",RGB(255,220,80)),
    EXTRA("MECHA-NARUTO",1080,0.155f,RGB(125,135,150),RGB(75,85,100),RGB(110,120,140),RGB(110,195,255),"ROCKET FIST","MECHA CLONES","LASER BURST","MECHA BIJUU BLAST",480,1,"OVERDRIVE MODE",RGB(100,200,255)),
    EXTRA("SASUKE CURSE MARK",980,0.165f,RGB(70,65,90),RGB(70,55,100),RGB(30,25,45),RGB(155,100,220),"CURSE CHIDORI","WING SLASH","FLAME CONTROL","CURSE MARK SUSANOO",440,1,"CURSE MARK STAGE 2",RGB(155,85,225)),
    EXTRA("SASUKE ADULT",1000,0.160f,RGB(45,45,55),RGB(65,55,75),RGB(20,20,25),RGB(160,155,255),"CHIDORI SPEAR","AMENOTEJIKARA","AMATERASU","COMPLETE BODY SUSANOO",490,1,"ADULT RINNEGAN",RGB(155,145,255)),
    EXTRA("KAKASHI YOUNG",880,0.165f,RGB(55,75,115),RGB(45,60,85),RGB(185,185,195),RGB(145,195,255),"YOUNG CHIDORI","EARTH STYLE WALL","SHARINGAN COPY","LIGHTNING BEAST",400,0,"NONE",RGB(255,255,255)),
    EXTRA("KAKASHI DOUBLE SHARINGAN",980,0.175f,RGB(55,75,125),RGB(45,60,90),RGB(185,185,200),RGB(155,200,255),"KAMUI LIGHTNING","SUSANOO SHURIKEN","KAMUI RAIKIRI","DOUBLE KAMUI SUSANOO",500,1,"DOUBLE MANGEKYO",RGB(155,205,255)),
    EXTRA("KAKASHI SIXTH HOKAGE",930,0.150f,RGB(70,80,100),RGB(55,65,80),RGB(190,190,200),RGB(150,190,230),"LIGHTNING BLADE","NINKEN","KAMUI","HOKAGE LIGHTNING CUT",430,0,"NONE",RGB(255,255,255)),
    EXTRA("MIGHT GUY EIGHT GATES",1120,0.195f,RGB(50,125,60),RGB(40,35,40),RGB(25,25,25),RGB(255,80,45),"DAYTIME TIGER","EVENING ELEPHANT","RED STEAM RUSH","NIGHT GUY",560,1,"EIGHT GATES RELEASED",RGB(255,65,45)),
    EXTRA("HASHIRAMA SAGE",1200,0.135f,RGB(145,45,45),RGB(65,65,65),RGB(30,25,20),RGB(80,215,100),"SAGE WOOD DRAGON","WOOD GOLEM","THOUSAND HANDS","TRUE SEVERAL THOUSAND HANDS",560,1,"SAGE MODE",RGB(255,130,70)),
    EXTRA("MADARA SIX PATHS",1250,0.145f,RGB(110,35,45),RGB(65,65,80),RGB(20,20,25),RGB(245,115,50),"LIMBO CLONE","TRUTH SEEKER ORB","RINNEGAN FLAME","SIX PATHS METEOR",570,1,"TEN-TAILS JINCHURIKI",RGB(235,235,255)),
    EXTRA("OBITO SIX PATHS",1180,0.155f,RGB(80,65,100),RGB(55,45,80),RGB(30,25,35),RGB(165,85,225),"TRUTH SEEKER ORB","SIX PATHS ROD","TEN-TAILS BLAST","SIX PATHS CATACLYSM",550,1,"TEN-TAILS FORM",RGB(170,90,230)),
    EXTRA("MINATO REANIMATED",1010,0.175f,RGB(225,185,65),RGB(235,230,215),RGB(240,200,75),RGB(255,225,105),"FLYING RAIJIN","REANIMATED RASENGAN","YELLOW FLASH","FLYING RAIJIN LEVEL 2",490,1,"KURAMA CHAKRA MODE",RGB(255,190,55)),
    EXTRA("ITACHI REANIMATED",960,0.150f,RGB(45,35,50),RGB(155,45,60),RGB(25,20,30),RGB(185,55,80),"AMATERASU","TSUKUYOMI","CROW CLONE","SUSANOO TOTSUKA BLADE",490,1,"REANIMATED SUSANOO",RGB(220,70,80)),
    EXTRA("DEIDARA REANIMATED",940,0.160f,RGB(195,195,195),RGB(45,45,50),RGB(225,225,225),RGB(230,190,100),"CLAY BIRD","C2 DRAGON","CLAY MINES","C0 ART IS AN EXPLOSION",470,1,"REANIMATED CLAY",RGB(230,190,100)),
    EXTRA("ZABUZA REANIMATED",1080,0.160f,RGB(75,85,95),RGB(55,60,70),RGB(40,40,45),RGB(145,180,215),"SILENT KILL","WATER DRAGON","HIDDEN MIST","DEMONIC EXECUTION",470,1,"REANIMATED DEMON",RGB(155,190,225)),
    EXTRA("HAKU UNMASKED",880,0.170f,RGB(185,205,220),RGB(70,115,155),RGB(45,45,55),RGB(150,215,255),"ICE MIRRORS","SENBON RAIN","ICE NEEDLE STORM","DEMONIC ICE MIRRORS",410,0,"NONE",RGB(255,255,255)),
    EXTRA("GAARA WAR",1000,0.135f,RGB(150,125,95),RGB(130,45,45),RGB(100,65,45),RGB(210,160,100),"SAND COFFIN","SAND SHOWER","SAND TSUNAMI","SHUKAKU SAND MAUSOLEUM",470,1,"KAZEKAGE SAND ARMOR",RGB(200,155,100)),
    EXTRA("FOURTH RAIKAGE MODE",1150,0.190f,RGB(70,80,90),RGB(45,50,60),RGB(205,205,210),RGB(130,195,255),"LIGHTNING LARIAT","LIGER BOMB","GUILLOTINE DROP","RAIKAGE FULL-POWER LARIAT",510,1,"LIGHTNING CHAKRA MODE",RGB(110,190,255)),
    EXTRA("BORUTO CHUNIN",970,0.180f,RGB(65,115,195),RGB(45,55,100),RGB(55,85,170),RGB(105,165,255),"VANISHING RASENGAN","SHADOW CLONE RUSH","LIGHTNING PALM","CHUNIN EXAM COMBO",440,1,"KARMA BURST",RGB(100,170,255)),
    EXTRA("SARADA CHUNIN",920,0.170f,RGB(180,55,70),RGB(35,35,45),RGB(45,35,40),RGB(225,100,125),"SHARINGAN STRIKE","FIREBALL","CHIDORI","CHUNIN EXAM COMBO",430,1,"SHARINGAN AWAKENING",RGB(235,105,130)),
    EXTRA("NARUTO SCHOOL COSTUME",930,0.145f,RGB(60,100,200),RGB(220,220,230),RGB(245,205,55),RGB(100,160,255),"RASENGAN","CLONE FEINT","SPINNING KICK","SCHOOL DAYS COMBO",390,0,"NONE",RGB(255,255,255)),
    EXTRA("SASUKE BLACK OUTFIT",950,0.150f,RGB(35,35,45),RGB(40,40,50),RGB(20,20,25),RGB(145,160,255),"CHIDORI","FIREBALL","CURSE MARK STRIKE","DARK SUSANOO SLASH",430,1,"CURSE MARK STAGE 2",RGB(145,80,220))
#undef EXTRA
};

static const char *unlock_hint[NUM_CHARS] = {
    "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", ""
};

/* ---------------- story data ---------------- */
typedef struct { const char *spk, *txt; } Line;
typedef struct { float r, dmg, spd; const char *msg; int vis; } Phase;
typedef struct {
    const char *title, *objective;
    int chapter, pdef, edef, arena, req;
    float boss_hp;
    Phase ph[2];
    int pre_n;
    Line pre[5];
    int post_n;
    Line post[3];
    int unlock_char;
    int psup[2], esup[2];
} Mission;

/* NOTE: indices are stored in save files. Only ever append new missions. */
#define NUM_MISSIONS 6
static const Mission missions[NUM_MISSIONS] = {
    { "HASHIRAMA VS MADARA", "DEFEAT MADARA", 1, 2, 3, 1, -1, 1.0f,
      { { 0.6f, 1.25f, 1.1f, "MADARA: SUSANOO!", 1 }, { 0.3f, 1.5f, 1.2f, "MADARA: SIX PATHS POWER!", 2 } },
      5, { { "NARRATOR",  "Two legends of the shinobi world clash once more at the Valley of the End." },
           { "MADARA",    "Hashirama... I will show you the power of the Uchiha." },
           { "HASHIRAMA", "Then I will end this here, Madara. For the sake of the village." },
           { "MADARA",    "Come, then! Show me what the First Hokage can do!" },
           { "HASHIRAMA", "Get ready. This battle decides everything." } },
      2, { { "HASHIRAMA", "Madara... you were always my greatest rival." },
           { "NARRATOR",  "The legendary duel ends. But the world is about to be swept into war." } },
      3, { -1, -1 }, { -1, -1 } },

    { "THE FOURTH GREAT NINJA WAR", "HOLD THE LINE", 2, 4, 3, 0, 0, 1.3f,
      { { 0.5f, 1.3f, 1.1f, "MADARA: SUSANOO!", 1 }, { 0.0f, 1.0f, 1.0f, "", 0 } },
      4, { { "NARRATOR", "The Fourth Great Ninja War begins. The Allied Shinobi Forces rise against an unstoppable enemy." },
           { "KAKASHI",  "Hold the line, everyone! We cannot let him reach the others." },
           { "MADARA",   "Your alliance is a fragile thing. Let me show you how fragile." },
           { "KAKASHI",  "Then we will prove you wrong. Sakura, Naruto, stand with me!" } },
      2, { { "KAKASHI",  "He is down... but this war is far from over." },
           { "NARRATOR", "The Allied Forces gain precious time. The greatest battles are still to come." } },
      4, { 5, 0 }, { -1, -1 } },

    { "NARUTO VS SASUKE", "SETTLE THE RIVALRY", 6, 0, 1, 2, 4, 1.4f,
      { { 0.66f, 1.2f, 1.1f, "SASUKE: SUSANOO!", 1 }, { 0.33f, 1.4f, 1.2f, "SASUKE: RINNEGAN POWER!", 2 } },
      3, { { "NARRATOR", "At the Valley of the End, two rivals face each other one last time." },
           { "SASUKE",   "Naruto... this ends today. Show me everything you have." },
           { "NARUTO",   "I'm not holding back either, Sasuke. Let's finish this!" } },
      3, { { "SASUKE",   "...I lost. You really did get stronger, Naruto." },
           { "NARUTO",   "We both did. That's what rivals are for." },
           { "NARRATOR", "The rivalry is settled, and their bond endures." } },
      -1, { -1, -1 }, { -1, -1 } },

    { "OBITO AND THE TEN-TAILS", "DEFEAT OBITO", 3, 0, 6, 1, 1, 1.5f,
      { { 0.6f, 1.3f, 1.1f, "OBITO: TEN-TAILS POWER!", 1 }, { 0.3f, 1.5f, 1.2f, "OBITO: JINCHURIKI FORM!", 2 } },
      3, { { "NARRATOR", "On the battlefield, the masked man reveals himself, wielding the power of the Ten-Tails." },
           { "OBITO",    "A world of dreams is the only peace that is real." },
           { "NARUTO",   "I won't let you take everyone's future! Sasuke, Kakashi-sensei, stay with me!" } },
      2, { { "OBITO",    "...I wanted to believe in that world. Perhaps I was wrong." },
           { "NARRATOR", "The Ten-Tails' host falls, but the shadow of the moon is growing." } },
      6, { 1, 4 }, { -1, -1 } },

    { "MADARA UCHIHA", "DEFEAT MADARA", 4, 1, 3, 1, 3, 1.5f,
      { { 0.6f, 1.25f, 1.1f, "MADARA: SUSANOO!", 1 }, { 0.3f, 1.5f, 1.2f, "MADARA: SIX PATHS POWER!", 2 } },
      3, { { "NARRATOR", "Madara, revived with the power of the Ten-Tails, stands above the battlefield." },
           { "MADARA",   "I am the one who will bring this world to its knees." },
           { "SASUKE",   "Naruto, cover me. I'll handle him." } },
      2, { { "SASUKE",   "He's gone... but something far worse is awakening." },
           { "NARRATOR", "The Rabbit Goddess is about to return." } },
      -1, { 0, 5 }, { -1, -1 } },

    { "KAGUYA OTSUTSUKI", "DEFEAT KAGUYA", 5, 0, 7, 3, 4, 1.2f,
      { { 0.66f, 1.2f, 1.1f, "KAGUYA: DIMENSION SHIFT!", 1 }, { 0.33f, 1.4f, 1.2f, "KAGUYA: TRUTH-SEEKING FURY!", 2 } },
      3, { { "NARRATOR", "Kaguya Otsutsuki, the mother of chakra, descends. Team 7 stands together once more." },
           { "KAGUYA",   "All of you will return to me. Sleep." },
           { "NARUTO",   "Not a chance! Team 7, let's do this together!" } },
      2, { { "NARUTO",   "We did it... but the real fight is still ahead." },
           { "NARRATOR", "With Kaguya sealed, only one battle remains." } },
      7, { 1, 5 }, { -1, -1 } }
};

#define NUM_ROWS 6
static const char *row_title[NUM_ROWS] = {
    "1 HASHIRAMA VS MADARA", "2 FOURTH GREAT NINJA WAR", "3 OBITO AND THE TEN-TAILS",
    "4 MADARA UCHIHA", "5 KAGUYA OTSUTSUKI", "6 NARUTO VS SASUKE"
};
static const int row_mission[NUM_ROWS] = { 0, 1, 3, 4, 5, 2 };

/* ---------------- save data ---------------- */
typedef struct { unsigned magic, version, completed, crc; } SaveData;
#define SAVE_MAGIC 0x4E533450u
#define SAVE_DIR "ms0:/PSP/SAVEDATA/NARUTOSTORM4"
#define SAVE_FILE "ms0:/PSP/SAVEDATA/NARUTOSTORM4/save.bin"
static SaveData sv;

static unsigned save_crc(const SaveData *s)
{
    return (s->magic ^ (s->version << 8) ^ s->completed) * 2654435761u + 12345u;
}

static int save_write(void)
{
    SaveData s = sv;
    int fd, ok;
    s.magic = SAVE_MAGIC; s.version = 1; s.crc = save_crc(&s);
    sceIoMkdir("ms0:/PSP", 0777);
    sceIoMkdir("ms0:/PSP/SAVEDATA", 0777);
    sceIoMkdir(SAVE_DIR, 0777);
    fd = sceIoOpen(SAVE_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return 0;
    ok = (sceIoWrite(fd, &s, sizeof(s)) == (int)sizeof(s));
    sceIoClose(fd);
    return ok;
}

static void save_load(void)
{
    SaveData s;
    int fd;
    memset(&sv, 0, sizeof(sv));
    fd = sceIoOpen(SAVE_FILE, PSP_O_RDONLY, 0777);
    if (fd < 0) return;
    if (sceIoRead(fd, &s, sizeof(s)) == (int)sizeof(s) && s.magic == SAVE_MAGIC &&
        s.version == 1 && s.crc == save_crc(&s)) {
        sv = s;
        sv.completed &= (1u << NUM_MISSIONS) - 1u;
    }
    sceIoClose(fd);
}

static int mission_open(int i)
{
    return missions[i].req < 0 || ((sv.completed >> missions[i].req) & 1u);
}

static unsigned chars_unlocked(void)
{
    /* All entries in this prototype roster are available in Free Battle. */
    return 0xFFFFFFFFu;
}

static int character_unlocked(int i)
{
    return i >= 0 && i < NUM_CHARS;
}

/* ---------------- fighter ---------------- */
typedef enum { FS_IDLE, FS_ATTACK, FS_JUTSU, FS_CHARGE, FS_DASH, FS_ULT, FS_TEAM, FS_AWAKEN } FState;

typedef struct {
    int def, is_ai;
    float x, y, z, vy, fx, fz;
    float hp, maxhp, chakra, dmg_mult, spd_mult, awm, supm;
    FState state;
    int timer, combo, combo_win, queued, stun, inv, guarding, dead, hit_flash, phase_lvl;
    int move, hitdone, frozen, burn, awk, dash_chase;
    int jcd[3];
    int sup[2], sup_idx, sup_t, sup_def;
    float kbx, kbz, dashx, dashz, phase, burn_dps, mark_x, mark_z;
    int ai_cd, ai_mode, ai_mode_t, ai_guard, ai_charge;
} Fighter;

typedef struct {
    float ax, ay;
    int atk, jutsu, charge, guard, jump, dash, chakra_dash, substitute, ult, awaken, support, team;
} Input;

typedef struct {
    int active, life, dot, homing, stun;
    float x, y, z, vx, vz, speed, size, dmg, dotdmg, kb;
    unsigned col;
    Fighter *src, *tgt;
} Proj;

typedef struct { float x, y, z, vx, vy, vz; int life; unsigned col; } Particle;

static Fighter pl, en;
static Proj projs[MAXP];
static Particle parts[MAXPART];
static float shake = 0.0f;

/* ---------------- game state ---------------- */
typedef enum {
    GS_MENU, GS_SELECT, GS_STORY, GS_DIALOGUE, GS_BATTLE, GS_RESULT, GS_COMPLETE
} GameState;
static GameState gstate = GS_MENU;
static int menu_sel = 0, select_idx = 0, select_mode = 0, story_mode = 0;
static int cur_mission = 0, cur_arena = 0, story_row = 0, reset_confirm = 0;
static int dlg_idx = 0, dlg_chars = 0, dlg_post = 0, cine_timer = 0;
static int intro = 0, ko_timer = 0, paused = 0, player_won = 0, result_sel = 0, save_ok = 0;
static unsigned frame = 0;
static char msg[48] = "";
static int msg_t = 0;
static char note[56] = "";
static int note_t = 0;
static float camx, camy, camz, tgx, tgy, tgz;
static float menu_angle = 0.0f;

static const unsigned sky_top[NUM_ARENAS] = {
    RGB(50, 110, 220), RGB(70, 50, 110), RGB(25, 20, 55), RGB(10, 0, 30)
};
static const unsigned sky_bot[NUM_ARENAS] = {
    RGB(175, 220, 255), RGB(250, 165, 95), RGB(150, 95, 135), RGB(95, 30, 115)
};

static void set_msg(const char *s) { strncpy(msg, s, 47); msg[47] = 0; msg_t = 120; }
static void set_note(const char *s) { strncpy(note, s, 55); note[55] = 0; note_t = 150; }

static float dist_xz(float ax, float az, float bx, float bz)
{
    float dx = ax - bx, dz = az - bz;
    return sqrtf(dx * dx + dz * dz);
}

/* ---------------- effects ---------------- */
static void spawn_fx(float x, float y, float z, unsigned col, int n)
{
    int i, k = 0;
    for (i = 0; i < MAXPART && k < n; i++) {
        if (parts[i].life <= 0) {
            Particle *p = &parts[i];
            p->x = x; p->y = y; p->z = z;
            p->vx = ((rand() % 200) - 100) * 0.0015f;
            p->vy = 0.04f + (rand() % 100) * 0.0012f;
            p->vz = ((rand() % 200) - 100) * 0.0015f;
            p->life = 18 + rand() % 12;
            p->col = col;
            k++;
        }
    }
}

static void fx_update(void)
{
    int i;
    for (i = 0; i < MAXPART; i++) {
        Particle *p = &parts[i];
        if (p->life <= 0) continue;
        p->x += p->vx; p->y += p->vy; p->z += p->vz;
        p->vy -= 0.004f;
        p->life--;
    }
    shake *= 0.85f;
}

/* ---------------- fighter logic ---------------- */
static void make_fighter(Fighter *f, int def, float x, float z, int is_ai)
{
    memset(f, 0, sizeof(*f));
    f->def = def; f->is_ai = is_ai;
    f->x = x; f->z = z;
    f->maxhp = f->hp = (float)roster[def].hp;
    f->chakra = 50.0f;
    f->supm = 50.0f;
    f->sup[0] = f->sup[1] = -1;
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
        spawn_fx(t->x, t->y + 1.4f, t->z, RGB(120, 190, 255), 3);
        if (t->hp <= 0) { t->hp = 0; t->dead = 1; }
        return 2;
    }
    t->hp -= dmg;
    if (t->hp <= 0) { t->hp = 0; t->dead = 1; }
    if (src->awk == 0) src->awm = fminf(100.0f, src->awm + dmg * 0.12f);
    if (t->awk == 0) t->awm = fminf(100.0f, t->awm + dmg * 0.08f);
    t->stun = stun; t->kbx = dx / len * kb; t->kbz = dz / len * kb;
    t->state = FS_IDLE; t->combo = 0; t->queued = 0; t->hit_flash = 6;
    if (dmg >= 100.0f) {
        spawn_fx(t->x, t->y + 1.4f, t->z, RGB(255, 170, 40), 14);
        if (shake < 0.22f) shake = 0.22f;
    } else {
        spawn_fx(t->x, t->y + 1.4f, t->z, RGB(255, 255, 190), 6);
        if (shake < 0.05f) shake = 0.05f;
    }
    return 1;
}

static void melee_hit(Fighter *f, Fighter *o)
{
    /* Storm-style rising pressure across the four-hit basic combo. */
    static const float dmg[5] = { 0, 18, 20, 26, 62 };
    static const int stn[5] = { 0, 8, 8, 12, 26 };
    static const float kb[5] = { 0, 0.06f, 0.06f, 0.12f, 0.48f };
    int c = f->combo, hit;
    if (c < 1 || c > 4) return;
    if (dist_xz(f->x, f->z, o->x, o->z) <= 2.4f && fabsf(f->y - o->y) < 1.6f) {
        hit = deal_damage(o, f, dmg[c], stn[c], kb[c]);
        if (hit == 1) f->chakra = fminf(100.0f, f->chakra + (c == 4 ? 8.0f : 4.0f));
        if (hit == 1 && c == 3) {
            spawn_fx(o->x, o->y + 1.2f, o->z, RGB(220,235,255), 10);
            if (shake < 0.10f) shake = 0.10f;
        }
        if (hit == 1 && c == 4) {
            spawn_fx(o->x, o->y + 1.3f, o->z, roster[f->def].orb, 18);
            if (shake < 0.22f) shake = 0.22f;
        }
    }
}

static Proj *spawn_proj(Fighter *f, Fighter *o, const Move *m, float ang, float side, float fwd,
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
            return p;
        }
    }
    return NULL;
}

/* support partner strike: homing projectile launched from the partner's position */
static void assist_strike(Fighter *f, Fighter *o)
{
    static const Move am = { "ASSIST", MV_HOMING, 0, 0, 85, 16, 0.35f, 0.0f, 0.2f, 0.7f };
    Proj *p = spawn_proj(f, o, &am, 0.0f, 0.0f, 0.0f, 1, 85.0f, 0.7f, 0);
    if (p) {
        float rx = -f->fz, rz = f->fx;
        p->x = f->x + rx * 2.2f + f->fx * 0.5f;
        p->z = f->z + rz * 2.2f + f->fz * 0.5f;
        p->col = roster[f->sup_def].orb;
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
        if (t == 30) { f->hp = fminf(f->maxhp, f->hp + (float)m->dmg); spawn_fx(f->x, f->y + 2.0f, f->z, RGB(120, 255, 150), 10); }
        if (t >= 40) f->state = FS_IDLE;
        break;
    case MV_PHASE:
        if (t == 1) f->inv = 44;
        if (t >= 44) f->state = FS_IDLE;
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

/* ultimate / awakening / support; returns 1 if the state changed */
static int try_special(Fighter *f, const Input *in)
{
    const Def *d = &roster[f->def];
    if (in->team && f->chakra >= 70.0f && f->supm >= 100.0f &&
        (f->sup[0] >= 0 || f->sup[1] >= 0) && f->sup_t == 0) {
        f->state = FS_TEAM; f->timer = 0; f->inv = 12;
        f->supm = 0.0f;
        set_msg("TEAM ULTIMATE!");
        return 1;
    }
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
    if (in->support && f->supm >= 100.0f && f->sup_t == 0) {
        int s = f->sup[f->sup_idx];
        if (s < 0) { f->sup_idx ^= 1; s = f->sup[f->sup_idx]; }
        if (s >= 0) {
            char b[48];
            f->sup_def = s; f->sup_t = 1; f->supm = 0.0f; f->sup_idx ^= 1;
            sprintf(b, "SUPPORT: %s", roster[s].name);
            set_msg(b);
        }
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
        f->supm = fminf(100.0f, f->supm + 0.06f);
        if (f->awk > 0) f->awk--; else f->awm = fminf(100.0f, f->awm + 0.01f);
        if (f->sup_t > 0) {
            f->sup_t++;
            if (f->sup_t == 20) assist_strike(f, o);
            if (f->sup_t > 50) f->sup_t = 0;
        }

        if (in->substitute && f->chakra >= 10.0f && f->stun > 0) {
            /* Storm-style substitution: spend chakra, escape hit-stun and reappear behind opponent. */
            f->chakra -= 10.0f;
            f->stun = 0; f->inv = 24; f->state = FS_IDLE; f->combo = 0; f->queued = 0;
            f->x = o->x - f->fx * 1.65f; f->z = o->z - f->fz * 1.65f;
            f->kbx = f->kbz = 0.0f;
            spawn_fx(f->x, f->y + 1.0f, f->z, RGB(210, 210, 235), 10);
            set_msg("SUBSTITUTION JUTSU!");
        } else if (f->stun > 0) {
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
                if (in->jump && !in->chakra_dash && f->y <= 0.001f) f->vy = 0.24f;
                if (in->charge && !in->guard && !in->chakra_dash) { f->state = FS_CHARGE; f->timer = 0; }
                else if (in->atk) {
                    f->combo = (f->combo_win > 0 && f->combo < 4) ? f->combo + 1 : 1;
                    f->combo_win = 0; f->queued = 0; f->state = FS_ATTACK; f->timer = 0;
                } else if (in->jutsu > 0) {
                    start_jutsu(f, in->jutsu - 1);
                } else if (in->chakra_dash && f->chakra >= 15.0f) {
                    /* Chakra dash: fast homing approach that can open a combo. */
                    f->chakra -= 15.0f;
                    f->dashx = f->fx; f->dashz = f->fz; f->dash_chase = 1;
                    f->hitdone = 0; f->state = FS_DASH; f->timer = 0; f->inv = 5;
                    spawn_fx(f->x, f->y + 0.8f, f->z, roster[f->def].orb, 5);
                } else if (in->dash) {
                    /* R-trigger remains a quick evasive ninja move. */
                    float m2 = sqrtf(mx * mx + mz * mz);
                    if (m2 > 0.3f) { f->dashx = mx / m2; f->dashz = mz / m2; }
                    else { f->dashx = -f->fx; f->dashz = -f->fz; }
                    f->dash_chase = 0; f->state = FS_DASH; f->timer = 0; f->inv = 12;
                }
            } break;
            case FS_ATTACK:
                f->timer++;
                if (f->combo >= 2 && f->timer >= 3 && f->timer <= 10 && in->jutsu > 0) {
                    start_jutsu(f, in->jutsu - 1);
                    if (f->state == FS_JUTSU) break;
                }
                if (f->combo >= 2 && in->team && try_special(f, in)) break;
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
                if (f->dash_chase) {
                    /* Slightly steer the dash toward the locked-on opponent. */
                    float tx = o->x - f->x, tz = o->z - f->z, tl = sqrtf(tx * tx + tz * tz);
                    if (tl > 0.01f) { f->dashx = tx / tl; f->dashz = tz / tl; }
                    f->x += f->dashx * 0.52f; f->z += f->dashz * 0.52f;
                    if (!f->hitdone && dist_xz(f->x, f->z, o->x, o->z) <= 1.55f) {
                        f->hitdone = 1;
                        if (deal_damage(o, f, 28.0f, 10, 0.12f) == 1) {
                            f->state = FS_ATTACK; f->timer = 0; f->combo = 1;
                            f->combo_win = 0; f->queued = 0;
                            spawn_fx(o->x, o->y + 1.3f, o->z, RGB(160, 210, 255), 9);
                        }
                    }
                    if (f->timer >= 12 && f->state == FS_DASH) f->state = FS_IDLE;
                } else {
                    f->x += f->dashx * 0.45f; f->z += f->dashz * 0.45f;
                    if (f->timer >= 8) f->state = FS_IDLE;
                }
                break;
            case FS_ULT:
                f->timer++;
                if (!o->dead) o->frozen = 1;
                if (f->timer == 1) { f->chakra = 0.0f; o->inv = 0; }
                if ((f->def == 4 || f->def == 5) && f->timer >= 40 && f->timer < 60 && dist > 1.4f) {
                    f->x += f->fx * 0.5f; f->z += f->fz * 0.5f;
                }
                if (f->timer == 60) {
                    deal_damage(o, f, (float)d->ult_dmg, 30, 0.6f);
                    spawn_fx(o->x, o->y + 1.4f, o->z, d->orb, 20);
                    shake = 0.5f;
                }
                if (f->timer >= 100) { o->frozen = 0; f->state = FS_IDLE; }
                break;
            case FS_TEAM:
                f->timer++;
                if (!o->dead) o->frozen = 1;
                if (f->timer == 1) { f->chakra -= 70.0f; o->inv = 0; }
                if (f->timer == 18) { deal_damage(o, f, 55.0f, 14, 0.16f); spawn_fx(o->x, o->y + 1.5f, o->z, RGB(100,190,255), 10); shake = 0.16f; }
                if (f->timer == 40) { deal_damage(o, f, 85.0f, 18, 0.24f); spawn_fx(o->x, o->y + 1.5f, o->z, roster[f->def].orb, 14); shake = 0.25f; }
                if (f->timer == 66) { deal_damage(o, f, 235.0f, 30, 0.65f); spawn_fx(o->x, o->y + 1.5f, o->z, RGB(255,235,180), 24); shake = 0.48f; }
                if (f->timer >= 98) { o->frozen = 0; f->state = FS_IDLE; }
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
static int ai_move_ok(const Move *m, float dist, float hpr, const Fighter *o)
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
    case MV_PHASE:   return dist < 4.5f && (o->state == FS_ATTACK || o->state == FS_JUTSU);
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
        if (f->chakra >= (float)m->cost && f->jcd[k] == 0 && ai_move_ok(m, dist, f->hp / f->maxhp, o))
            in->jutsu = k + 1;
    }
    if (f->chakra >= 100.0f && dist < 12.0f && rand() % 100 < 6) in->ult = 1;
    if (f->awm >= 100.0f && d->has_awk && f->awk == 0 && rand() % 100 < 10) in->awaken = 1;
    if (f->supm >= 100.0f && (f->sup[0] >= 0 || f->sup[1] >= 0) && dist > 3.0f && rand() % 100 < 3) in->support = 1;
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
    in.team = !!((p->Buttons & PSP_CTRL_LTRIGGER) && (pressed & PSP_CTRL_CIRCLE));
    in.atk = !!(pressed & PSP_CTRL_CIRCLE) && !in.team;
    if (pressed & PSP_CTRL_SQUARE) {
        if (p->Buttons & PSP_CTRL_LTRIGGER) in.support = 1;
        else {
            in.jutsu = 1;
            if (p->Buttons & PSP_CTRL_UP) in.jutsu = 2;
            else if (p->Buttons & PSP_CTRL_DOWN) in.jutsu = 3;
        }
    }
    in.jump = !!(pressed & PSP_CTRL_CROSS);
    in.dash = !!(pressed & PSP_CTRL_RTRIGGER);
    in.chakra_dash = !!((p->Buttons & PSP_CTRL_TRIANGLE) && (pressed & PSP_CTRL_CROSS));
    in.substitute = ((p->Buttons & PSP_CTRL_LTRIGGER) && (p->Buttons & PSP_CTRL_RTRIGGER) &&
                     ((pressed & PSP_CTRL_LTRIGGER) || (pressed & PSP_CTRL_RTRIGGER)));
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
    if (pl.state == FS_ULT || pl.state == FS_TEAM) return &pl;
    if (en.state == FS_ULT || en.state == FS_TEAM) return &en;
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
    } else if (cine_timer > 0) {
        float rx = -en.fz, rz = en.fx;
        k = 0.2f;
        ex = en.x + en.fx * 4.0f + rx * 2.5f; ey = 2.2f; ez = en.z + en.fz * 4.0f + rz * 2.5f;
        cx = en.x; cy = 1.9f; cz = en.z;
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
static void battle_start(int pdef, int edef, int story, int arena, float boss_hp,
                         const int *psup, const int *esup)
{
    int ps[2], es[2];
    ps[0] = psup ? psup[0] : -1; ps[1] = psup ? psup[1] : -1;
    es[0] = esup ? esup[0] : -1; es[1] = esup ? esup[1] : -1;
    memset(projs, 0, sizeof(projs));
    memset(parts, 0, sizeof(parts));
    shake = 0.0f;
    make_fighter(&pl, pdef, -4.0f, 0.0f, 0);
    make_fighter(&en, edef, 4.0f, 0.0f, 1);
    pl.sup[0] = ps[0]; pl.sup[1] = ps[1];
    en.sup[0] = es[0]; en.sup[1] = es[1];
    en.maxhp *= boss_hp; en.hp = en.maxhp;
    story_mode = story; cur_arena = arena;
    intro = 0; ko_timer = 0; paused = 0; msg_t = 0; cine_timer = 0;
    srand(frame * 2654435761u + 7u);
    camera_update(1);
    gstate = GS_BATTLE;
}

static void start_mission_battle(void)
{
    const Mission *m = &missions[cur_mission];
    battle_start(m->pdef, m->edef, 1, m->arena, m->boss_hp, m->psup, m->esup);
}

static void start_mission_dialogue(int mi)
{
    cur_mission = mi;
    dlg_post = 0; dlg_idx = 0; dlg_chars = 0;
    if (missions[mi].pre_n > 0) gstate = GS_DIALOGUE;
    else start_mission_battle();
}

static void finish_mission(void)
{
    sv.completed |= (1u << cur_mission);
    save_ok = save_write();
    gstate = GS_COMPLETE;
}

/* random unlocked character that is neither a nor b; -1 if none found */
static int pick_support(int a, int b)
{
    int tries, s;
    for (tries = 0; tries < 40; tries++) {
        s = rand() % NUM_CHARS;
        if (s != a && s != b && character_unlocked(s)) return s;
    }
    return -1;
}

static void start_free_battle(int pdef)
{
    int e, ps[2], es[2];
    srand(frame);
    do { e = rand() % NUM_CHARS; } while (e == pdef);
    ps[0] = pick_support(pdef, e);
    ps[1] = pick_support(pdef, e);
    if (ps[1] == ps[0]) ps[1] = -1;
    es[0] = pick_support(pdef, e);
    es[1] = pick_support(pdef, e);
    if (es[1] == es[0]) es[1] = -1;
    battle_start(pdef, e, 0, rand() % NUM_ARENAS, 1.0f, ps, es);
}

static void battle_update(const SceCtrlData *pad, unsigned pressed)
{
    Input pin, ein;
    int i;
    float dx, dz, dist;

    if (pressed & PSP_CTRL_START) paused = !paused;
    if (paused) {
        if (pressed & PSP_CTRL_SELECT) { paused = 0; gstate = story_mode ? GS_STORY : GS_MENU; }
        return;
    }
    if (msg_t > 0) msg_t--;
    fx_update();
    intro++;
    if (intro < 90) { camera_update(0); return; }
    if (cine_timer > 0) { cine_timer--; camera_update(0); return; }

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
        const Mission *m = &missions[cur_mission];
        float ratio = en.hp / en.maxhp;
        for (i = 0; i < 2; i++) {
            const Phase *ph = &m->ph[i];
            if (ph->r > 0.0f && ratio <= ph->r && en.phase_lvl < ph->vis) {
                en.phase_lvl = ph->vis; en.dmg_mult = ph->dmg; en.spd_mult = ph->spd;
                en.awm = 100.0f;
                set_msg(ph->msg);
                cine_timer = CINE_FRAMES;
                msg_t = CINE_FRAMES + 45;
                shake = 0.3f;
            }
        }
    }

    camera_update(0);

    if (pl.dead || en.dead) {
        if (++ko_timer > 110) {
            player_won = en.dead && !pl.dead;
            result_sel = 0;
            if (story_mode && player_won) {
                const Mission *m = &missions[cur_mission];
                dlg_post = 1; dlg_idx = 0; dlg_chars = 0;
                if (m->post_n > 0) gstate = GS_DIALOGUE; else finish_mission();
            } else gstate = GS_RESULT;
        }
    }
}

/* ---------------- rendering: characters ---------------- */
static void part(const Fighter *f, float rxo, float yo, float fzo, float w, float h, float dd, unsigned col)
{
    float rx = -f->fz, rz = f->fx;
    gfx_box_rot(f->x + rx * rxo + f->fx * fzo, f->y + yo, f->z + rz * rxo + f->fz * fzo,
                w, h, dd, atan2f(f->fx, f->fz), col);
}

/* hair, face and outfit details that make each character recognisable */
static void draw_details(const Fighter *f, unsigned hair, unsigned dark)
{
    int id = f->def;
    unsigned cloth = RGB(30, 50, 170), silver = RGB(195, 200, 212);

    switch (id) {
    case 0: /* Naruto: spiky blond hair, headband, whiskers, zip stripe */
        part(f, -0.20f, 2.66f, 0.0f, 0.16f, 0.30f, 0.16f, hair);
        part(f,  0.00f, 2.72f, 0.0f, 0.16f, 0.36f, 0.16f, hair);
        part(f,  0.20f, 2.66f, 0.0f, 0.16f, 0.30f, 0.16f, hair);
        part(f, -0.36f, 2.50f, 0.0f, 0.14f, 0.26f, 0.40f, hair);
        part(f,  0.36f, 2.50f, 0.0f, 0.14f, 0.26f, 0.40f, hair);
        part(f, 0.0f, 2.30f, 0.0f, 0.64f, 0.13f, 0.58f, cloth);
        part(f, 0.0f, 2.30f, 0.30f, 0.30f, 0.12f, 0.04f, silver);
        part(f, -0.30f, 2.06f, 0.20f, 0.03f, 0.03f, 0.14f, dark);
        part(f,  0.30f, 2.06f, 0.20f, 0.03f, 0.03f, 0.14f, dark);
        part(f, -0.30f, 2.13f, 0.20f, 0.03f, 0.03f, 0.14f, dark);
        part(f,  0.30f, 2.13f, 0.20f, 0.03f, 0.03f, 0.14f, dark);
        part(f, 0.0f, 1.5f, 0.23f, 0.18f, 0.55f, 0.04f, RGB(25, 50, 190));
        break;
    case 1: /* Sasuke: dark swept hair, headband, high collar */
        part(f, 0.00f, 2.40f, -0.34f, 0.50f, 0.50f, 0.20f, hair);
        part(f, -0.20f, 2.62f, -0.20f, 0.16f, 0.30f, 0.30f, hair);
        part(f,  0.20f, 2.62f, -0.20f, 0.16f, 0.30f, 0.30f, hair);
        part(f, -0.18f, 2.22f, 0.27f, 0.18f, 0.18f, 0.04f, hair);
        part(f,  0.18f, 2.22f, 0.27f, 0.18f, 0.18f, 0.04f, hair);
        part(f, 0.0f, 1.78f, -0.05f, 0.62f, 0.26f, 0.5f, RGB(235, 235, 235));
        break;
    case 2: /* Hashirama: long dark hair, red armor with shoulder plates */
        part(f, -0.34f, 1.95f, -0.12f, 0.12f, 1.00f, 0.42f, hair);
        part(f,  0.34f, 1.95f, -0.12f, 0.12f, 1.00f, 0.42f, hair);
        part(f, 0.00f, 1.90f, -0.32f, 0.60f, 1.10f, 0.12f, hair);
        part(f, -0.56f, 1.68f, 0.0f, 0.34f, 0.14f, 0.42f, RGB(70, 70, 75));
        part(f,  0.56f, 1.68f, 0.0f, 0.34f, 0.14f, 0.42f, RGB(70, 70, 75));
        part(f, 0.0f, 1.45f, 0.23f, 0.7f, 0.5f, 0.05f, RGB(190, 60, 55));
        break;
    case 3: /* Madara: long spiky hair, red battle armor */
        part(f, 0.00f, 1.95f, -0.34f, 0.70f, 1.10f, 0.14f, hair);
        part(f, -0.42f, 2.30f, -0.25f, 0.18f, 0.60f, 0.30f, hair);
        part(f,  0.42f, 2.30f, -0.25f, 0.18f, 0.60f, 0.30f, hair);
        part(f, -0.20f, 2.70f, 0.0f, 0.16f, 0.30f, 0.16f, hair);
        part(f,  0.20f, 2.70f, 0.0f, 0.16f, 0.30f, 0.16f, hair);
        part(f, 0.18f, 2.30f, 0.28f, 0.26f, 0.40f, 0.05f, hair);
        part(f, 0.0f, 1.45f, 0.23f, 0.72f, 0.52f, 0.06f, RGB(160, 45, 55));
        part(f, -0.58f, 1.68f, 0.0f, 0.36f, 0.20f, 0.46f, RGB(160, 45, 55));
        part(f,  0.58f, 1.68f, 0.0f, 0.36f, 0.20f, 0.46f, RGB(160, 45, 55));
        break;
    case 4: /* Kakashi: silver spiky hair, face mask, slanted headband */
        part(f,  0.00f, 2.72f, 0.0f, 0.20f, 0.30f, 0.20f, hair);
        part(f, -0.26f, 2.60f, 0.0f, 0.20f, 0.30f, 0.20f, hair);
        part(f,  0.26f, 2.60f, 0.0f, 0.20f, 0.30f, 0.20f, hair);
        part(f, 0.0f, 1.98f, 0.28f, 0.54f, 0.28f, 0.05f, RGB(35, 45, 70));
        part(f, 0.0f, 2.30f, 0.0f, 0.64f, 0.13f, 0.58f, cloth);
        part(f, 0.14f, 2.14f, 0.29f, 0.24f, 0.22f, 0.04f, cloth);
        part(f, 0.0f, 1.5f, 0.23f, 0.7f, 0.58f, 0.04f, RGB(80, 100, 70));
        break;
    case 5: /* Sakura: pink bob, red headband, red top with white emblem */
        part(f, -0.32f, 2.15f, 0.0f, 0.10f, 0.50f, 0.56f, hair);
        part(f,  0.32f, 2.15f, 0.0f, 0.10f, 0.50f, 0.56f, hair);
        part(f,  0.00f, 2.20f, -0.30f, 0.60f, 0.60f, 0.10f, hair);
        part(f, 0.0f, 2.30f, 0.0f, 0.64f, 0.12f, 0.58f, RGB(200, 40, 60));
        part(f, 0.0f, 1.5f, 0.23f, 0.18f, 0.18f, 0.04f, RGB(250, 250, 250));
        break;
    case 6: /* Obito: orange mask, dark spiky hair, dark cloak */
        part(f, 0.00f, 2.12f, 0.28f, 0.54f, 0.54f, 0.05f, RGB(255, 120, 40));
        part(f, 0.12f, 2.16f, 0.31f, 0.10f, 0.10f, 0.02f, dark);
        part(f,  0.00f, 2.70f, 0.0f, 0.20f, 0.30f, 0.20f, hair);
        part(f, -0.26f, 2.60f, 0.0f, 0.20f, 0.30f, 0.20f, hair);
        part(f,  0.26f, 2.60f, 0.0f, 0.20f, 0.30f, 0.20f, hair);
        part(f, 0.0f, 1.20f, -0.32f, 0.95f, 1.90f, 0.10f, RGB(35, 35, 45));
        break;
    case 7: /* Kaguya: long white hair, horns, third eye, robe */
        part(f, 0.0f, 1.50f, -0.34f, 0.80f, 2.00f, 0.12f, hair);
        part(f, 0.0f, 2.32f, 0.28f, 0.10f, 0.10f, 0.03f, RGB(220, 40, 60));
        part(f, -0.18f, 2.62f, 0.0f, 0.10f, 0.30f, 0.10f, RGB(255, 255, 255));
        part(f,  0.18f, 2.62f, 0.0f, 0.10f, 0.30f, 0.10f, RGB(255, 255, 255));
        break;
    case 8: /* Minato: swept blond hair and Hokage cloak */
        part(f, -0.24f, 2.62f, -0.02f, 0.18f, 0.28f, 0.18f, hair);
        part(f, 0.02f, 2.70f, -0.02f, 0.18f, 0.32f, 0.18f, hair);
        part(f, 0.28f, 2.57f, -0.02f, 0.16f, 0.24f, 0.16f, hair);
        part(f, 0.0f, 1.48f, -0.30f, 0.96f, 1.15f, 0.16f, RGB(245,240,220));
        part(f, 0.0f, 1.55f, -0.40f, 0.72f, 0.75f, 0.08f, RGB(185,45,45));
        break;
    case 9: /* Itachi: dark Akatsuki cloak and red cloud markings */
        part(f, 0.0f, 1.42f, 0.0f, 0.92f, 0.95f, 0.50f, RGB(25,22,30));
        part(f, 0.0f, 2.30f, 0.0f, 0.65f, 0.13f, 0.58f, RGB(35,35,45));
        part(f, -0.22f, 2.65f, -0.05f, 0.16f, 0.32f, 0.16f, hair);
        part(f, 0.22f, 2.65f, -0.05f, 0.16f, 0.32f, 0.16f, hair);
        part(f, 0.0f, 1.45f, 0.28f, 0.18f, 0.50f, 0.04f, RGB(150,35,45));
        break;
    case 10: /* Pain: orange hair and Rinnegan markers */
        part(f, -0.22f, 2.62f, -0.05f, 0.14f, 0.32f, 0.14f, hair);
        part(f, 0.22f, 2.62f, -0.05f, 0.14f, 0.32f, 0.14f, hair);
        part(f, 0.0f, 2.50f, -0.28f, 0.52f, 0.28f, 0.10f, hair);
        part(f, -0.18f, 2.12f, 0.29f, 0.04f, 0.04f, 0.04f, RGB(190,190,200));
        part(f, 0.18f, 2.12f, 0.29f, 0.04f, 0.04f, 0.04f, RGB(190,190,200));
        break;
    case 11: /* Gaara: red hair and sand gourd */
        part(f, 0.0f, 2.62f, -0.03f, 0.64f, 0.22f, 0.58f, hair);
        part(f, 0.0f, 1.45f, -0.38f, 0.65f, 0.92f, 0.35f, RGB(120,65,45));
        part(f, 0.0f, 1.86f, 0.25f, 0.18f, 0.14f, 0.04f, RGB(205,155,95));
        break;
    case 12: /* Might Guy: bowl-cut hair and green outfit */
        part(f, 0.0f, 2.42f, 0.0f, 0.62f, 0.18f, 0.58f, hair);
        part(f, -0.16f, 2.12f, 0.29f, 0.08f, 0.06f, 0.04f, dark);
        part(f, 0.16f, 2.12f, 0.29f, 0.08f, 0.06f, 0.04f, dark);
        part(f, 0.0f, 1.45f, 0.24f, 0.52f, 0.08f, 0.04f, RGB(35,35,40));
        break;
    case 13: /* Jiraiya: long white hair */
        part(f, -0.34f, 2.12f, -0.10f, 0.12f, 1.10f, 0.30f, hair);
        part(f, 0.34f, 2.12f, -0.10f, 0.12f, 1.10f, 0.30f, hair);
        part(f, 0.0f, 2.64f, -0.08f, 0.56f, 0.24f, 0.52f, hair);
        part(f, -0.28f, 2.08f, 0.28f, 0.04f, 0.08f, 0.04f, RGB(190,40,45));
        part(f, 0.28f, 2.08f, 0.28f, 0.04f, 0.08f, 0.04f, RGB(190,40,45));
        break;
    case 14: /* Orochimaru: straight black hair and pale robe */
        part(f, 0.0f, 2.64f, -0.04f, 0.60f, 0.25f, 0.56f, hair);
        part(f, -0.30f, 2.20f, -0.08f, 0.12f, 0.85f, 0.30f, hair);
        part(f, 0.30f, 2.20f, -0.08f, 0.12f, 0.85f, 0.30f, hair);
        part(f, 0.0f, 1.50f, -0.30f, 0.92f, 0.95f, 0.12f, RGB(220,220,220));
        break;
    case 15: /* Killer Bee: head wrap and dark glasses */
        part(f, 0.0f, 2.42f, 0.0f, 0.62f, 0.18f, 0.58f, dark);
        part(f, -0.14f, 2.14f, 0.29f, 0.15f, 0.10f, 0.04f, RGB(15,15,20));
        part(f, 0.14f, 2.14f, 0.29f, 0.15f, 0.10f, 0.04f, RGB(15,15,20));
        part(f, -0.30f, 1.25f, 0.05f, 0.06f, 0.95f, 0.06f, RGB(45,45,50));
        part(f, 0.30f, 1.25f, 0.05f, 0.06f, 0.95f, 0.06f, RGB(45,45,50));
        break;
    }
}

static void draw_fighter(const Fighter *f)
{
    const Def *d = &roster[f->def];
    unsigned body = d->body, acc = d->accent, hair = d->hair, skin = RGB(242, 205, 165);
    unsigned dark = RGB(25, 25, 35), eyec = RGB(25, 25, 35), belt = RGB(35, 30, 35);
    float sw = sinf(f->phase) * 0.35f;
    int i, id = f->def;
    const Move *mv = (f->state == FS_JUTSU) ? &d->jut[f->move] : NULL;

    if (f->hit_flash > 0) { body = RGB(255, 255, 255); acc = body; }
    if (id == 1 || id == 3 || id == 6) eyec = RGB(225, 30, 35);
    if (id == 5) eyec = RGB(60, 170, 90);
    if (id == 7) eyec = RGB(170, 150, 220);

    gfx_box(f->x, 0.03f, f->z, 1.3f, 0.02f, 1.3f, RGB(20, 20, 20));

    if (f->dead) {
        gfx_box_rot(f->x, 0.28f, f->z, 0.9f, 0.55f, 1.9f, atan2f(f->fx, f->fz), body);
        part(f, 0.0f, 0.32f, 1.25f, 0.55f, 0.5f, 0.52f, skin);
        part(f, 0.0f, 0.55f, 1.2f, 0.62f, 0.16f, 0.58f, hair);
        return;
    }
    if (f->phase_lvl >= 1) part(f, 0.0f, 1.9f, -0.7f, 2.4f, 3.6f, 0.3f, id == 6 ? RGB(160, 60, 220) : RGB(90, 110, 255));
    if (f->phase_lvl >= 2) part(f, 0.0f, 4.0f, -0.7f, 1.4f, 0.3f, 0.3f, RGB(255, 255, 255));

    if (f->awk > 0) {
        for (i = 0; i < 4; i++) {
            float a = frame * 0.1f + i * 1.57f, h = 1.6f + 0.5f * sinf(frame * 0.3f + i);
            gfx_box(f->x + cosf(a) * 0.95f, h * 0.5f, f->z + sinf(a) * 0.95f, 0.15f, h, 0.15f, d->awk_col);
        }
        if (id == 1) part(f, 0.0f, 1.9f, -0.7f, 2.4f, 3.6f, 0.3f, d->awk_col);
        if (id == 3) part(f, 0.0f, 3.4f, -0.5f, 1.6f, 0.3f, 0.3f, d->awk_col);
        if (id == 2) part(f, 0.0f, 2.2f, 0.27f, 0.5f, 0.1f, 0.05f, d->awk_col);
        if (id == 6) part(f, 0.0f, 3.2f, -0.6f, 2.0f, 0.4f, 0.4f, d->awk_col);
    }
    if (f->burn > 0) {
        for (i = 0; i < 3; i++)
            gfx_box(f->x + sinf(frame * 0.7f + i * 2.0f) * 0.35f, 0.6f + ((frame + i * 3) % 9) * 0.2f,
                    f->z + cosf(frame * 0.5f + i * 2.0f) * 0.35f, 0.3f, 0.3f, 0.3f, RGB(45, 0, 70));
    }

    /* legs, feet (Kaguya wears a long robe instead) */
    if (id == 7) {
        part(f, 0.0f, 0.65f, 0.0f, 1.0f, 1.3f, 0.7f, body);
        part(f, 0.0f, 0.2f, 0.0f, 1.1f, 0.2f, 0.8f, acc);
    } else {
        gfx_ellipsoid(f->x - 0.20f, 0.53f, f->z + sw * 0.35f, 0.31f, 0.78f, 0.34f, acc);
        gfx_ellipsoid(f->x + 0.20f, 0.53f, f->z - sw * 0.35f, 0.31f, 0.78f, 0.34f, acc);
        part(f, -0.20f, 0.10f, sw + 0.06f, 0.30f, 0.18f, 0.46f, dark);
        part(f,  0.20f, 0.10f, -sw + 0.06f, 0.30f, 0.18f, 0.46f, dark);
    }
    /* belt, torso, collar/neck */
    part(f, 0.0f, 1.02f, 0.0f, 0.80f, 0.16f, 0.46f, belt);
    gfx_ellipsoid(f->x, 1.43f, f->z, 0.82f, 0.78f, 0.50f, body);
    part(f, 0.0f, 1.62f, 0.23f, 0.52f, 0.14f, 0.04f, acc);
    part(f, 0.0f, 1.82f, 0.0f, 0.22f, 0.14f, 0.22f, skin);
    /* costume panel, seams and waist trim break up flat colors */
    part(f, 0.0f, 1.38f, 0.255f, 0.46f, 0.34f, 0.035f, acc);
    part(f, -0.29f, 1.48f, 0.276f, 0.035f, 0.40f, 0.025f, dark);
    part(f,  0.29f, 1.48f, 0.276f, 0.035f, 0.40f, 0.025f, dark);
    part(f, 0.0f, 1.08f, 0.245f, 0.58f, 0.055f, 0.035f, RGB(205,175,95));
    /* low-poly shin guards and sandal straps */
    part(f, -0.20f, 0.43f, 0.19f, 0.27f, 0.42f, 0.035f, dark);
    part(f,  0.20f, 0.43f, 0.19f, 0.27f, 0.42f, 0.035f, dark);
    part(f, -0.20f, 0.18f, 0.27f, 0.28f, 0.055f, 0.035f, acc);
    part(f,  0.20f, 0.18f, 0.27f, 0.28f, 0.055f, 0.035f, acc);
    /* shoulders */
    gfx_ellipsoid(f->x - 0.52f, 1.68f, f->z, 0.36f, 0.32f, 0.40f, body);
    gfx_ellipsoid(f->x + 0.52f, 1.68f, f->z, 0.36f, 0.32f, 0.40f, body);

    /* head: face, eyes, hair cap */
    gfx_ellipsoid(f->x, 2.12f, f->z, 0.62f, 0.66f, 0.58f, skin);
    if (id != 6) {
        /* brows, eyes and a small nose bridge make faces read better at gameplay distance */
        part(f, -0.145f, 2.205f, 0.285f, 0.15f, 0.035f, 0.035f, hair);
        part(f,  0.145f, 2.205f, 0.285f, 0.15f, 0.035f, 0.035f, hair);
        part(f, -0.14f, 2.14f, 0.292f, 0.105f, 0.065f, 0.035f, id == 4 ? skin : eyec);
        part(f,  0.14f, 2.14f, 0.292f, 0.105f, 0.065f, 0.035f, eyec);
        part(f, 0.0f, 2.04f, 0.302f, 0.055f, 0.075f, 0.025f, RGB(220,165,130));
    }
    gfx_ellipsoid(f->x, 2.43f, f->z - 0.02f, 0.70f, 0.30f, 0.64f, hair);
    /* layered hair locks give the base silhouette more anime-like depth */
    part(f, -0.23f, 2.58f, 0.02f, 0.17f, 0.34f, 0.18f, hair);
    part(f,  0.23f, 2.58f, 0.02f, 0.17f, 0.34f, 0.18f, hair);
    part(f, 0.0f, 2.64f, -0.12f, 0.18f, 0.34f, 0.20f, hair);

    draw_details(f, hair, dark);

    /* Low-cost costume pass: raised seams, wrist guards, gloves and boot caps.
       These are separate depth-layered pieces so the silhouette reads better
       without requiring texture maps or expensive PSP shaders. */
    part(f, 0.0f, 1.22f, 0.285f, 0.48f, 0.035f, 0.025f, RGB(225, 205, 145));
    part(f, -0.37f, 1.48f, 0.22f, 0.055f, 0.42f, 0.035f, d->accent);
    part(f,  0.37f, 1.48f, 0.22f, 0.055f, 0.42f, 0.035f, d->accent);
    part(f, -0.55f, 1.10f, 0.08f, 0.29f, 0.18f, 0.31f, dark);
    part(f,  0.55f, 1.10f, 0.08f, 0.29f, 0.18f, 0.31f, dark);
    part(f, -0.55f, 1.04f, 0.13f, 0.25f, 0.10f, 0.27f, acc);
    part(f,  0.55f, 1.04f, 0.13f, 0.25f, 0.10f, 0.27f, acc);
    gfx_ellipsoid(f->x - 0.20f, 0.16f, f->z + sw * 0.35f + 0.06f,
                  0.20f, 0.13f, 0.28f, dark);
    gfx_ellipsoid(f->x + 0.20f, 0.16f, f->z - sw * 0.35f + 0.06f,
                  0.20f, 0.13f, 0.28f, dark);

    /* Arms */
    if ((f->state == FS_ATTACK && f->timer >= 3 && f->timer <= 9) ||
        (mv && (mv->type == MV_DASH || mv->type == MV_SLASH) && f->timer >= 4 && f->timer <= 16)) {
        part(f, -0.50f, 1.50f, 0.70f, 0.22f, 0.22f, 1.10f, body);
        part(f, -0.50f, 1.50f, 1.32f, 0.28f, 0.28f, 0.28f, skin);
        part(f,  0.52f, 1.30f, 0.10f, 0.22f, 0.60f, 0.24f, body);
        part(f,  0.52f, 0.95f, 0.10f, 0.20f, 0.20f, 0.20f, skin);
    } else if (f->guarding) {
        part(f, -0.15f, 1.58f, 0.45f, 0.58f, 0.22f, 0.22f, body);
        part(f,  0.15f, 1.40f, 0.45f, 0.58f, 0.22f, 0.22f, body);
        part(f, -0.50f, 1.58f, 0.45f, 0.24f, 0.24f, 0.24f, skin);
        part(f,  0.50f, 1.40f, 0.45f, 0.24f, 0.24f, 0.24f, skin);
    } else {
        gfx_ellipsoid(f->x - 0.55f, 1.39f, f->z - sw * 0.25f, 0.27f, 0.58f, 0.30f, body);
        gfx_ellipsoid(f->x + 0.55f, 1.39f, f->z + sw * 0.25f, 0.27f, 0.58f, 0.30f, body);
        /* forearm guards and glove cuffs, with low polygon cost */
        part(f, -0.56f, 1.14f, 0.08f, 0.25f, 0.12f, 0.28f, acc);
        part(f,  0.56f, 1.14f, 0.08f, 0.25f, 0.12f, 0.28f, acc);
        gfx_ellipsoid(f->x - 0.56f, 1.02f, f->z - sw * 0.25f, 0.22f, 0.22f, 0.22f, skin);
        gfx_ellipsoid(f->x + 0.56f, 1.02f, f->z + sw * 0.25f, 0.22f, 0.22f, 0.22f, skin);
    }

    if (f->state == FS_ATTACK) {
        int t = f->timer;
        /* Give combo hits different silhouettes: cross-kick, spinning sweep, then heavy finisher. */
        if (f->combo == 2 && t >= 3 && t <= 9) {
            part(f, 0.22f, 0.62f, 0.64f, 0.25f, 0.24f, 0.92f, acc);
            part(f, 0.22f, 0.62f, 1.10f, 0.30f, 0.26f, 0.30f, dark);
        } else if (f->combo == 3 && t >= 3 && t <= 9) {
            gfx_box_rot(f->x + f->fx * 1.1f, 1.45f, f->z + f->fz * 1.1f,
                        2.3f, 0.12f, 1.4f, atan2f(f->fx, f->fz) + frame * 0.08f, d->orb);
        } else if (f->combo == 4 && t >= 2 && t <= 10) {
            part(f, 0.0f, 1.45f, 1.05f, 0.72f, 0.72f, 0.85f, d->orb);
            if ((t & 1) == 0) part(f, 0.0f, 1.45f, 1.45f, 0.44f, 0.44f, 0.50f, RGB(255,255,235));
        }
    }

    if (mv) {
        int t = f->timer;
        switch (mv->type) {
        case MV_DASH:
            part(f, -0.5f, 1.5f, 1.6f, 0.5f, 0.5f, 0.5f, d->orb);
            part(f, -0.5f, 1.5f, 1.6f, 0.7f, 0.2f, 0.2f, RGB(255, 255, 255));
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
                c.state = FS_ATTACK; c.timer = 5; c.hit_flash = 0; c.awk = 0; c.burn = 0; c.sup_t = 0;
                draw_fighter(&c);
                c.x = f->x - (-f->fz) * 1.4f; c.z = f->z - f->fx * 1.4f;
                draw_fighter(&c);
            }
            break;
        case MV_WARP:
            if (t >= 3 && t <= 8) part(f, 0.0f, 1.5f, 0.0f, 1.4f, 2.4f, 1.4f, RGB(160, 80, 220));
            break;
        case MV_PHASE:
            if ((t / 3) & 1) part(f, 0.0f, 1.4f, 0.0f, 1.4f, 2.8f, 1.4f, RGB(150, 60, 220));
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

/* support partner ghost, drawn next to the caller while the assist plays */
static void draw_support(const Fighter *f)
{
    Fighter g;
    if (f->sup_t <= 0 || f->dead) return;
    memset(&g, 0, sizeof(g));
    g.def = f->sup_def;
    g.x = f->x + (-f->fz) * 2.2f + f->fx * 0.5f;
    g.z = f->z + f->fx * 2.2f + f->fz * 0.5f;
    g.fx = f->fx; g.fz = f->fz;
    g.state = (f->sup_t >= 12 && f->sup_t <= 26) ? FS_ATTACK : FS_IDLE;
    g.timer = 5;
    draw_fighter(&g);
}

/* big visuals for each Ultimate Jutsu */
static void draw_ult_fx(const Fighter *c, const Fighter *o)
{
    const Def *d = &roster[c->def];
    int t = c->timer;
    float dist = dist_xz(c->x, c->z, o->x, o->z);
    float s;

    if (c->state == FS_TEAM) {
        float pulse = 1.0f + 0.08f * (float)(t % 8);
        float midx = (c->x + o->x) * 0.5f, midz = (c->z + o->z) * 0.5f;
        if (t < 66) {
            part(c, 0.0f, 1.8f, 0.8f, pulse, pulse, pulse, d->orb);
            gfx_box(midx, 1.5f, midz, 0.55f + t * 0.025f, 0.35f + t * 0.01f, dist * 0.9f, d->orb);
            if ((t / 4) & 1) gfx_box(o->x, 1.45f, o->z, 1.2f + t * 0.015f, 1.8f, 1.2f + t * 0.015f, RGB(255,255,235));
        } else if (t < 82) {
            float flash = 1.0f + (t - 66) * 0.5f;
            gfx_box(o->x, 1.4f, o->z, flash, flash, flash, RGB(255,255,255));
            part(c, 0.0f, 1.7f, 0.0f, flash, flash, flash, RGB(255,220,100));
        }
        return;
    }
    switch (c->def) {
    case 0:
        s = 0.4f + t * 0.04f; if (s > 3.0f) s = 3.0f;
        if (t < 40) part(c, 0.0f, 2.8f, 0.5f, s, s, s, RGB(70, 30, 120));
        else if (t < 60) {
            float k = (t - 40) / 20.0f;
            gfx_box(c->x + (o->x - c->x) * k, 2.4f - k * 1.2f, c->z + (o->z - c->z) * k, s, s, s, RGB(70, 30, 120));
        }
        break;
    case 1:
    case 4:
    case 6:
        if (t >= 30 && t < 60) part(c, 0.0f, 1.6f, 0.9f, 0.25f + (t - 30) * 0.02f, 0.25f, 0.8f + (t - 30) * 0.05f, d->orb);
        if (t >= 60 && t < 72) gfx_box_rot(c->x + (o->x - c->x) * 0.5f, 1.4f, c->z + (o->z - c->z) * 0.5f,
                                           0.5f, 0.5f, dist, atan2f(c->fx, c->fz), d->orb);
        break;
    case 2:
        s = t * 0.18f; if (s > 8.0f) s = 8.0f;
        part(c, 0.0f, s * 0.5f, -2.0f, 4.0f, s, 2.5f, RGB(110, 190, 110));
        if (t >= 40 && t < 64) {
            float k = (t - 40) / 20.0f; if (k > 1.0f) k = 1.0f;
            gfx_box(c->x + (o->x - c->x) * k, 2.5f, c->z + (o->z - c->z) * k, 3.0f, 3.0f, 3.0f, RGB(130, 210, 130));
        }
        break;
    case 3:
    case 7:
        if (t < 20) part(c, 0.0f, 3.0f, 0.5f, 0.8f, 0.8f, 0.8f, d->orb);
        else if (t < 62) {
            float y = 18.0f - (t - 20) * 0.4f; if (y < 1.8f) y = 1.8f;
            gfx_box(o->x, y, o->z, 4.0f, 4.0f, 4.0f, d->orb);
        }
        break;
    case 8: /* Minato: yellow-flash teleport streaks */
        for (int j = 0; j < 3; j++) {
            float k = (float)(t + j * 8) / 64.0f;
            if (k > 1.0f) k = 1.0f;
            gfx_box_rot(c->x + (o->x - c->x) * k, 1.7f + 0.35f * sinf(t * 0.2f + j),
                        c->z + (o->z - c->z) * k, 0.22f, 0.22f, 1.7f,
                        atan2f(o->x - c->x, o->z - c->z), RGB(255,220,75));
        }
        if (t >= 52 && t < 70) part(o, 0.0f, 1.7f, 0.0f, 2.0f, 2.0f, 2.0f, RGB(255,245,180));
        break;
    case 9: /* Itachi: black-flame ring and red Susanoo blade */
        if (t < 60) {
            for (int j = 0; j < 5; j++) {
                float a = (float)j * 1.2566f + t * 0.06f;
                part(o, cosf(a) * 1.25f, 1.4f + 0.35f * sinf(t * 0.12f + j),
                     sinf(a) * 1.25f, 0.38f, 0.8f, 0.38f, RGB(35,8,28));
            }
        } else if (t < 78) {
            gfx_box_rot((c->x + o->x) * 0.5f, 1.6f, (c->z + o->z) * 0.5f,
                        0.28f, 0.28f, dist + 1.5f, atan2f(c->fx, c->fz), RGB(220,40,55));
        }
        break;
    case 10: /* Pain: Planetary Devastation */
        if (t < 48) {
            float q = 0.4f + t * 0.06f;
            gfx_box(o->x, 8.0f - t * 0.045f, o->z, q, q, q, RGB(65,55,75));
            part(o, 0.0f, 2.4f, 0.0f, q * 0.75f, q * 0.75f, q * 0.75f, RGB(155,145,180));
        } else if (t < 72) {
            float q = 1.0f + (t - 48) * 0.18f;
            gfx_box(o->x, 1.4f, o->z, q, q, q, RGB(100,85,130));
            part(o, 0.0f, 1.5f, 0.0f, q, q, q, RGB(220,220,240));
        }
        break;
    case 11: /* Gaara: sand mausoleum */
        if (t < 45) {
            float h = 0.3f + t * 0.08f;
            gfx_box(o->x, h * 0.5f, o->z, 1.7f + t * 0.025f, h, 1.7f + t * 0.025f, RGB(165,130,85));
            part(o, 0.0f, 0.8f + t * 0.025f, 0.0f, 0.9f, 0.9f, 0.9f, RGB(220,190,135));
        } else if (t < 75) {
            gfx_box(o->x, 1.6f, o->z, 2.8f, 3.2f, 2.8f, RGB(125,90,55));
            if ((t & 3) == 0) spawn_fx(o->x, 1.4f, o->z, RGB(210,175,105), 4);
        }
        break;
    case 12: /* Might Guy: red Eight Gates rush */
        if (t < 60) {
            float q = 1.1f + 0.04f * (float)(t % 10);
            part(c, 0.0f, 1.5f, 0.0f, q, 2.9f, q, RGB(230,45,35));
            gfx_box_rot(c->x + (o->x - c->x) * 0.55f, 1.5f, c->z + (o->z - c->z) * 0.55f,
                        0.45f, 0.45f, 2.6f, atan2f(c->fx, c->fz), RGB(255,95,45));
        }
        if (t >= 60 && t < 78) gfx_box(o->x, 1.5f, o->z, 3.5f, 3.5f, 3.5f, RGB(255,220,180));
        break;
    case 13: /* Jiraiya: sage-fire oil blast */
        if (t < 44) {
            float q = 0.45f + t * 0.055f;
            part(c, 0.0f, 2.1f, 0.6f, q, q, q, RGB(255,130,35));
            gfx_box_rot(c->x + c->fx * (1.0f + t * 0.04f), 1.8f,
                        c->z + c->fz * (1.0f + t * 0.04f), q, q, q,
                        atan2f(c->fx, c->fz), RGB(255,200,80));
        } else if (t < 72) {
            part(o, 0.0f, 1.7f, 0.0f, 2.0f, 2.4f, 2.0f, RGB(255,110,35));
            if ((t & 2) == 0) part(o, 0.0f, 2.0f, 0.0f, 1.0f, 1.0f, 1.0f, RGB(255,225,125));
        }
        break;
    case 14: /* Orochimaru: eight-branch white serpent coils */
        for (int j = 0; j < 6; j++) {
            float a = (float)j * 1.0472f + t * 0.035f;
            float rr = 0.9f + (t < 50 ? t * 0.025f : 1.25f);
            gfx_box_rot(o->x + cosf(a) * rr, 1.1f + 0.45f * sinf(a * 2.0f),
                        o->z + sinf(a) * rr, 0.55f, 0.45f, 1.2f,
                        -a, RGB(230,225,240));
        }
        break;
    case 15: /* Killer Bee: blue Gyuki tailed-beast bomb */
        if (t < 42) {
            float q = 0.5f + t * 0.06f;
            part(c, 0.0f, 2.0f, 0.8f, q, q, q, RGB(40,85,180));
            part(c, 0.0f, 2.0f, 0.8f, q * 0.62f, q * 0.62f, q * 0.62f, RGB(120,185,255));
        } else if (t < 70) {
            float k = (float)(t - 42) / 28.0f;
            gfx_box(o->x, 1.5f, o->z, 1.5f + k * 4.0f, 1.5f + k * 4.0f, 1.5f + k * 4.0f, RGB(40,85,180));
            part(o, 0.0f, 1.5f, 0.0f, 1.0f + k * 2.0f, 1.0f + k * 2.0f, 1.0f + k * 2.0f, RGB(150,210,255));
        }
        break;
    default:
        s = 0.3f + t * 0.02f;
        part(c, 0.0f, 1.5f, 0.9f, s, s, s, RGB(255, 120, 190));
        break;
    }
    if (t >= 60 && t < 74) {
        float e = (t - 60) * 0.45f + 1.0f;
        gfx_box(o->x, 1.2f, o->z, e, e, e, RGB(255, 255, 255));
    }
}

/* ---------------- rendering: arenas ---------------- */
static unsigned dim(unsigned c, int num)
{
    return RGB(((c & 255) * num) / 100, (((c >> 8) & 255) * num) / 100, (((c >> 16) & 255) * num) / 100);
}

static void draw_statue(float x, float dir, unsigned c)
{
    gfx_box(x, 1.0f, 0.0f, 7.0f, 2.0f, 7.0f, dim(c, 70));
    gfx_box(x, 6.5f, 0.0f, 4.0f, 9.0f, 3.0f, c);
    gfx_box(x, 12.8f, 0.0f, 2.6f, 2.8f, 2.6f, c);
    gfx_box(x, 14.4f, 0.0f, 3.0f, 0.5f, 3.0f, dim(c, 85));
    gfx_box(x, 8.0f, 2.6f, 1.3f, 6.0f, 1.3f, c);
    gfx_box(x, 8.0f, -2.6f, 1.3f, 6.0f, 1.3f, c);
    gfx_box(x + dir * 1.6f, 10.0f, 0.0f, 1.2f, 1.0f, 1.0f, c);
}

static void draw_arena(int style, float cx, float cz)
{
    static const unsigned ca[NUM_ARENAS] = { RGB(105, 130, 100), RGB(160, 138, 105), RGB(125, 125, 138), RGB(90, 62, 125) };
    static const unsigned cb[NUM_ARENAS] = { RGB(85, 108, 82), RGB(132, 112, 86), RGB(98, 98, 112), RGB(58, 40, 90) };
    static const unsigned cp[NUM_ARENAS] = { RGB(125, 125, 135), RGB(115, 104, 92), RGB(75, 75, 90), RGB(215, 215, 240) };
    static const unsigned bd[NUM_ARENAS] = { RGB(60, 72, 60), RGB(95, 78, 56), RGB(72, 72, 88), RGB(150, 150, 195) };
    static const unsigned gd[NUM_ARENAS] = { RGB(55, 88, 55), RGB(125, 98, 72), RGB(70, 66, 86), RGB(30, 15, 50) };
    static const unsigned mt[NUM_ARENAS] = { RGB(75, 105, 95), RGB(130, 92, 82), RGB(62, 56, 84), RGB(48, 22, 78) };
    int i, j;

    if (style < 0 || style >= NUM_ARENAS) style = 0;
    gfx_box(0, -0.4f, 0, 130.0f, 0.5f, 130.0f, gd[style]);
    gfx_box(0, -0.12f, 0, 23.6f, 0.3f, 23.6f, bd[style]);
    for (i = -3; i <= 3; i++)
        for (j = -3; j <= 3; j++)
            gfx_box(i * 3.2f, 0.02f, j * 3.2f, 3.1f, 0.04f, 3.1f, ((i + j) & 1) ? ca[style] : cb[style]);

    for (i = 0; i < 16; i++) {
        float ang = i * 0.3927f, h = 2.0f + (i % 3) * 1.2f;
        float px = cosf(ang) * 11.8f, pz = sinf(ang) * 11.8f;
        if (style == 3) h += 2.5f * (float)(i & 1);
        if (dist_xz(px, pz, cx, cz) < 5.5f) continue; /* never let a pillar block the view */
        gfx_box(px, h * 0.5f, pz, 0.9f, h, 0.9f, cp[style]);
    }
    for (i = 0; i < 12; i++) {
        float ang = i * 0.5236f + 0.2f, h = 9.0f + (float)((i * 7) % 5) * 4.0f;
        gfx_box(cosf(ang) * 62.0f, h * 0.5f, sinf(ang) * 62.0f, 20.0f, h, 20.0f, mt[style]);
    }
    if (style == 1 || style == 2) {
        draw_statue(-32.0f, 1.0f, style == 1 ? RGB(150, 138, 118) : RGB(110, 108, 124));
        draw_statue(32.0f, -1.0f, style == 1 ? RGB(150, 138, 118) : RGB(110, 108, 124));
    }
    if (style == 0) {
        for (i = 0; i < 8; i++) {
            float ang = i * 0.7854f + 0.4f, tx = cosf(ang) * 18.0f, tz = sinf(ang) * 18.0f;
            gfx_box(tx, 1.6f, tz, 0.9f, 3.2f, 0.9f, RGB(95, 65, 40));
            gfx_box(tx, 4.6f, tz, 4.2f, 3.0f, 4.2f, RGB(55, 135, 65));
            gfx_box(tx, 6.6f, tz, 2.6f, 1.6f, 2.6f, RGB(70, 155, 80));
        }
    }
    if (style == 3) {
        for (i = 0; i < 10; i++) {
            float ang = i * 0.6283f, r = 20.0f, h = 6.0f + (float)((i * 5) % 4) * 3.0f;
            float bob = sinf(frame * 0.03f + i) * 0.8f;
            gfx_box(cosf(ang) * r, h + bob, sinf(ang) * r, 1.6f, 2.6f, 1.6f, RGB(230, 230, 250));
        }
    }
}

static void draw_sky(int style)
{
    gfx_2d();
    gfx_gradient(0, 0, 480, 272, sky_top[style], sky_bot[style]);
}

/* ---------------- rendering: HUD ---------------- */
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
    if (pl.sup[0] >= 0 || pl.sup[1] >= 0) draw_bar(10, 56, 70, 3, pl.supm / 100.0f, RGB(120, 255, 150), 0);
    gfx_text(470 - gfx_text_width(en_name, 2), 6, 2, RGB(255, 255, 255), en_name);
    draw_bar(280, 24, 190, 10, en.hp / en.maxhp, hp_color(en.hp / en.maxhp), 1);
    draw_bar(340, 38, 130, 6, en.chakra / 100.0f, RGB(70, 150, 255), 1);
    draw_bar(370, 48, 100, 4, en.awk > 0 ? (float)en.awk / AWAKEN_FRAMES : en.awm / 100.0f,
             en.awk > 0 ? roster[en.def].awk_col : RGB(255, 200, 40), 1);
    if (en.sup[0] >= 0 || en.sup[1] >= 0) draw_bar(400, 56, 70, 3, en.supm / 100.0f, RGB(120, 255, 150), 1);
    if (pl.combo > 1) { sprintf(buf, "%d HIT", pl.combo); gfx_text(10, 70, 2, RGB(255, 220, 60), buf); }
    if (msg_t > 0 && cine_timer == 0) gfx_text(240 - gfx_text_width(msg, 2) / 2, 80, 2, RGB(255, 200, 60), msg);
    if (intro < 60) {
        gfx_text(240 - gfx_text_width("READY", 4) / 2, 110, 4, RGB(255, 255, 255), "READY");
        if (story_mode) {
            sprintf(buf, "OBJECTIVE: %s", missions[cur_mission].objective);
            gfx_text(240 - gfx_text_width(buf, 2) / 2, 150, 2, RGB(255, 220, 60), buf);
        }
    } else if (intro < 90) gfx_text(240 - gfx_text_width("FIGHT!", 4) / 2, 110, 4, RGB(255, 80, 60), "FIGHT!");
    if (pl.dead || en.dead) {
        const char *t = en.dead ? "VICTORY" : "DEFEATED";
        gfx_text(240 - gfx_text_width(t, 4) / 2, 110, 4, RGB(255, 220, 60), t);
    }

    x = 10;
    for (k = 0; k < 3; k++) {
        const Move *m = &pd->jut[k];
        int ready = (pl.chakra >= (float)m->cost && pl.jcd[k] == 0);
        sprintf(buf, "%s%s", pre[k], m->name);
        gfx_text(x, 234, 1, ready ? RGB(255, 255, 255) : RGB(150, 150, 150), buf);
        x += gfx_text_width(buf, 1) + 10;
    }
    if (pl.supm >= 100.0f && (pl.sup[0] >= 0 || pl.sup[1] >= 0))
        gfx_text(10, 222, 1, RGB(120, 255, 150), "SUPPORT READY: L + SQUARE");
    if (pl.supm >= 100.0f && pl.chakra >= 70.0f && (pl.sup[0] >= 0 || pl.sup[1] >= 0))
        gfx_text(10, 210, 1, RGB(255, 220, 100), "TEAM ULTIMATE READY: L + CIRCLE");
    if (pl.chakra >= 100.0f) gfx_text(10, 246, 1, RGB(255, 220, 60), "ULTIMATE READY: HOLD TRIANGLE + CIRCLE");
    else if (pl.awm >= 100.0f && pd->has_awk && pl.awk == 0) gfx_text(10, 246, 1, RGB(255, 200, 40), "AWAKENING READY: L + R + TRIANGLE");
    gfx_text(10, 262, 1, RGB(210, 210, 210), "O:ATK  X:JUMP  L:GUARD  R:DODGE  TRI:CHARGE  START:PAUSE");

    if (c || cine_timer > 0) {
        gfx_rect(0, 0, 480, 26, RGB(0, 0, 0));
        gfx_rect(0, 246, 480, 26, RGB(0, 0, 0));
    }
    if (c) {
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
    if (cine_timer > 0)
        gfx_text(240 - gfx_text_width(msg, 3) / 2, 100, 3, RGB(255, 80, 60), msg);
    if (paused) {
        gfx_rect(100, 90, 280, 80, RGBA(0, 0, 0, 220));
        gfx_text(240 - gfx_text_width("PAUSED", 3) / 2, 100, 3, RGB(255, 255, 255), "PAUSED");
        gfx_text(120, 140, 1, RGB(220, 220, 220), "START: RESUME   SELECT: QUIT");
    }
}

static void draw_team_combo_partners(const Fighter *f)
{
    int i;
    float rx = -f->fz, rz = f->fx;
    if (f->state != FS_TEAM || f->timer < 8 || f->timer > 82) return;
    for (i = 0; i < 2; i++) {
        Fighter g; int sid = f->sup[i];
        if (sid < 0 || sid >= NUM_CHARS) continue;
        memset(&g, 0, sizeof(g));
        g.def = sid;
        g.x = f->x + rx * (i == 0 ? -1.25f : 1.25f) - f->fx * 0.15f;
        g.z = f->z + rz * (i == 0 ? -1.25f : 1.25f) - f->fz * 0.15f;
        g.fx = f->fx; g.fz = f->fz; g.phase = frame * 0.12f + (float)i;
        g.state = FS_ATTACK; g.timer = (f->timer % 12) + 2;
        draw_fighter(&g);
    }
}

static void draw_battle(void)
{
    int i;
    Fighter *c = ult_caster();
    draw_sky(cur_arena);
    gfx_3d();
    gfx_camera(camx + sinf(frame * 1.7f) * shake, camy + cosf(frame * 2.3f) * shake, camz,
               tgx, tgy, tgz);
    draw_arena(cur_arena, camx, camz);
    draw_fighter(&pl);
    draw_fighter(&en);
    draw_team_combo_partners(&pl);
    draw_team_combo_partners(&en);
    draw_support(&pl);
    draw_support(&en);
    if (c) draw_ult_fx(c, (c == &pl) ? &en : &pl);
    for (i = 0; i < MAXP; i++)
        if (projs[i].active) {
            Proj *p = &projs[i];
            gfx_box(p->x, p->y, p->z, p->size + 0.2f, p->size + 0.2f, p->size + 0.2f, p->col);
            gfx_box(p->x - p->vx * 2.0f, p->y, p->z - p->vz * 2.0f, p->size * 0.6f, p->size * 0.6f, p->size * 0.6f, p->col);
        }
    for (i = 0; i < MAXPART; i++)
        if (parts[i].life > 0) {
            float s = 0.10f + parts[i].life * 0.005f;
            gfx_box(parts[i].x, parts[i].y, parts[i].z, s, s, s, parts[i].col);
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

static unsigned speaker_color(const char *s)
{
    int i;
    if (strcmp(s, "NARRATOR") == 0) return RGB(200, 200, 200);
    for (i = 0; i < NUM_CHARS; i++)
        if (strcmp(s, roster[i].name) == 0) return roster[i].orb;
    return RGB(255, 255, 255);
}

/* ---------------- main ---------------- */
int main(void)
{
    SceCtrlData pad;
    unsigned prev = 0, pressed;
    static const char *items[4] = { "STORY MODE", "FREE BATTLE", "CHARACTERS", "EXIT" };
    int i;

    setup_callbacks();
    gfx_init();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    save_load();

    for (;;) {
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;
        frame++;
        menu_angle += 0.02f;
        if (note_t > 0) note_t--;

        /* ---- update ---- */
        switch (gstate) {
        case GS_MENU:
            if (pressed & PSP_CTRL_UP)   menu_sel = (menu_sel + 3) % 4;
            if (pressed & PSP_CTRL_DOWN) menu_sel = (menu_sel + 1) % 4;
            if (pressed & PSP_CTRL_CROSS) {
                if (menu_sel == 3) sceKernelExitGame();
                else if (menu_sel == 0) { reset_confirm = 0; gstate = GS_STORY; }
                else { select_mode = (menu_sel == 2); gstate = GS_SELECT; }
            }
            break;
        case GS_SELECT:
            if (pressed & PSP_CTRL_LEFT)  select_idx = (select_idx + NUM_CHARS - 1) % NUM_CHARS;
            if (pressed & PSP_CTRL_RIGHT) select_idx = (select_idx + 1) % NUM_CHARS;
            if (pressed & PSP_CTRL_UP)    select_idx = (select_idx + NUM_CHARS - 6) % NUM_CHARS;
            if (pressed & PSP_CTRL_DOWN)  select_idx = (select_idx + 6) % NUM_CHARS;
            if (pressed & PSP_CTRL_CIRCLE) gstate = GS_MENU;
            if ((pressed & PSP_CTRL_CROSS) && !select_mode) {
                if (character_unlocked(select_idx)) start_free_battle(select_idx);
                else set_note("CHARACTER LOCKED - SEE STORY MODE");
            }
            break;
        case GS_STORY:
            if (pressed & PSP_CTRL_UP)   { story_row = (story_row + NUM_ROWS - 1) % NUM_ROWS; reset_confirm = 0; }
            if (pressed & PSP_CTRL_DOWN) { story_row = (story_row + 1) % NUM_ROWS; reset_confirm = 0; }
            if (pressed & PSP_CTRL_CIRCLE) gstate = GS_MENU;
            if (pressed & PSP_CTRL_SQUARE) {
                if (++reset_confirm >= 2) {
                    sv.completed = 0;
                    set_note(save_write() ? "PROGRESS ERASED" : "PROGRESS ERASED (SAVE FILE NOT WRITTEN)");
                    reset_confirm = 0;
                } else set_note("PRESS SQUARE AGAIN TO ERASE PROGRESS");
            }
            if (pressed & PSP_CTRL_CROSS) {
                int mi = row_mission[story_row];
                if (mi < 0) set_note("THIS CHAPTER IS NOT BUILT YET");
                else if (!mission_open(mi)) set_note("LOCKED - CLEAR THE PREVIOUS MISSION");
                else start_mission_dialogue(mi);
            }
            break;
        case GS_DIALOGUE: {
            const Mission *m = &missions[cur_mission];
            const Line *L = dlg_post ? m->post : m->pre;
            int n = dlg_post ? m->post_n : m->pre_n;
            int len = (int)strlen(L[dlg_idx].txt);
            int done = 0;
            if (dlg_chars < len) dlg_chars += 2;
            if (pressed & PSP_CTRL_CROSS) {
                if (dlg_chars < len) dlg_chars = len;
                else { dlg_idx++; dlg_chars = 0; if (dlg_idx >= n) done = 1; }
            }
            if (pressed & PSP_CTRL_START) done = 1;
            if (!dlg_post && (pressed & PSP_CTRL_CIRCLE)) gstate = GS_STORY;
            else if (done) { if (dlg_post) finish_mission(); else start_mission_battle(); }
        } break;
        case GS_BATTLE:
            battle_update(&pad, pressed);
            break;
        case GS_RESULT: {
            int cnt = story_mode ? 3 : 2;
            if (pressed & PSP_CTRL_UP)   result_sel = (result_sel + cnt - 1) % cnt;
            if (pressed & PSP_CTRL_DOWN) result_sel = (result_sel + 1) % cnt;
            if (pressed & PSP_CTRL_CROSS) {
                if (result_sel == 0) {
                    if (story_mode) start_mission_battle();
                    else battle_start(pl.def, en.def, 0, cur_arena, 1.0f, pl.sup, en.sup);
                } else if (story_mode && result_sel == 1) gstate = GS_STORY;
                else gstate = GS_MENU;
            }
        } break;
        case GS_COMPLETE:
            if (pressed & PSP_CTRL_CROSS) gstate = GS_STORY;
            break;
        }

        /* ---- render ---- */
        switch (gstate) {
        case GS_MENU: {
            static const char *descs[4] = {
                "FOLLOW THE SHINOBI STORY",
                "CHOOSE YOUR FIGHTER AND BATTLE",
                "VIEW THE AVAILABLE FIGHTERS",
                "RETURN TO PPSSPP"
            };
            unsigned pulse = (unsigned)(35 + (sinf(frame * 0.08f) + 1.0f) * 20.0f);
            gfx_begin(RGB(8, 10, 28));
            gfx_2d();
            gfx_gradient(0, 0, 480, 272, RGB(7, 10, 34), RGB(37, 20, 64));
            /* Animated background slashes and framed panels. */
            for (i = 0; i < 8; i++) {
                float x = (float)((i * 83 + (int)(frame * 0.45f)) % 540) - 60.0f;
                gfx_rect(x, 0, 1.0f, 272, RGBA(95, 115, 190, 24));
            }
            gfx_rect(8, 8, 464, 256, RGBA(4, 7, 22, 130));
            gfx_rect(8, 8, 3, 256, RGB(255, 145, 35));
            gfx_rect(15, 15, 450, 1, RGBA(255, 255, 255, 100));
            gfx_rect(15, 256, 450, 1, RGBA(255, 255, 255, 70));
            gfx_3d();
            gfx_camera(2.5f, 2.5f, 7.0f, 2.5f, 1.2f, 0.0f);
            gfx_box(2.5f, -0.05f, 0, 5.4f, 0.1f, 5.4f, RGB(46, 55, 90));
            draw_preview((int)(frame / 180) % 8, menu_angle, 2.5f);
            gfx_2d();
            gfx_text(24, 20, 3, RGB(255, 155, 35), "NARUTO");
            gfx_text(24, 43, 3, RGB(255, 255, 255), "STORM");
            gfx_text(24, 66, 3, RGB(255, 220, 85), "SHINOBI ARENA");
            gfx_rect(24, 94, 176, 2, RGB(255, 145, 35));
            gfx_text(24, 103, 1, RGB(170, 195, 255), "MAIN BATTLE MENU");
            for (i = 0; i < 4; i++) {
                float y = 125.0f + i * 27.0f;
                if (i == menu_sel) {
                    gfx_rect(20, y - 4, 184, 23, RGBA(255, 135, 30, 215));
                    gfx_rect(20, y - 4, 4, 23, RGB(255, 235, 155));
                    gfx_text(31, y + 2, 2, RGB(15, 17, 35), items[i]);
                } else {
                    gfx_rect(20, y - 4, 184, 23, RGBA(25, 32, 68, 150));
                    gfx_text(31, y + 2, 2, RGB(220, 225, 245), items[i]);
                }
            }
            gfx_rect(220, 205, 240, 36, RGBA(4, 7, 20, 205));
            gfx_text(230, 212, 1, RGB(255, 210, 115), descs[menu_sel]);
            gfx_text(230, 228, 1, RGB(180, 195, 225), "PSP HOMEbrew FIGHTING PROJECT");
            gfx_text(18, 264, 1, RGB(185, 195, 220), "UP/DOWN SELECT   CROSS CONFIRM");
            gfx_text(391, 20, 1, RGB(255, 220, 100), "BUILD 0.2");
            gfx_rect(455, 42, 2, 24 + (float)pulse * 0.3f, RGB(255, 150, 35));
            gfx_end();
            break;
        }
        case GS_SELECT: {
            char buf[64];
            const Def *sd = &roster[select_idx];
            int unlocked = character_unlocked(select_idx);
            int page_start = (select_idx / 24) * 24;
            int page_count = NUM_CHARS - page_start;
            if (page_count > 24) page_count = 24;
            gfx_begin(RGB(8, 12, 32));
            gfx_2d();
            gfx_gradient(0, 0, 480, 272, RGB(8, 13, 38), RGB(36, 20, 60));
            gfx_rect(0, 0, 480, 30, RGBA(4, 7, 20, 230));
            gfx_rect(0, 30, 480, 2, RGB(255, 145, 35));
            gfx_text(12, 8, 2, RGB(255, 220, 100), select_mode ? "FIGHTER DATABASE" : "SELECT YOUR FIGHTER");
            gfx_text(392, 10, 1, RGB(185, 200, 230), "ROSTER");
            /* Left roster panel: six columns by four rows, 24 entries per page. */
            gfx_rect(7, 39, 196, 187, RGBA(4, 7, 22, 215));
            for (i = 0; i < page_count; i++) {
                int idx = page_start + i;
                int col = i % 6, row = i / 6;
                float x = 12.0f + col * 31.0f;
                float y = 44.0f + row * 43.0f;
                unsigned tile = roster[idx].body;
                int chosen = (idx == select_idx);
                int open = character_unlocked(idx);
                gfx_rect(x, y, 27, 37, chosen ? RGB(255, 185, 45) : RGBA(120, 140, 190, 120));
                gfx_rect(x + 2, y + 2, 23, 33, chosen ? RGB(42, 36, 55) : RGBA(12, 18, 40, 235));
                /* Original colour-coded silhouette tile until real portraits are integrated. */
                gfx_rect(x + 8, y + 7, 11, 10, open ? roster[idx].hair : RGB(85, 85, 95));
                gfx_rect(x + 6, y + 18, 15, 10, open ? tile : RGB(65, 65, 75));
                gfx_rect(x + 4, y + 30, 19, 3, open ? roster[idx].accent : RGB(85, 65, 70));
                if (!open) gfx_rect(x + 2, y + 2, 23, 33, RGBA(0, 0, 0, 120));
                gfx_text(x + 2, y + 38, 1, chosen ? RGB(255, 235, 145) : RGB(190, 200, 225), idx < 99 ? (idx < 10 ? "0" : "") : "");
                if (chosen) {
                    gfx_rect(x - 2, y - 2, 31, 2, RGB(255, 245, 180));
                    gfx_rect(x - 2, y + 37, 31, 2, RGB(255, 245, 180));
                }
            }
            gfx_text(12, 232, 1, RGB(185, 200, 230), "D-PAD: MOVE   L/R: PAGE THROUGH FIGHTERS");
            gfx_text(12, 245, 1, RGB(255, 220, 100), "CROSS: CONFIRM   CIRCLE: BACK");
            sprintf(buf, "PAGE %d / %d", page_start / 24 + 1, (NUM_CHARS + 23) / 24);
            gfx_text(12, 258, 1, RGB(160, 180, 220), buf);
            /* Right-side fighter preview and information panel. */
            gfx_rect(211, 39, 262, 187, RGBA(4, 7, 22, 175));
            gfx_3d();
            gfx_camera(2.45f, 2.1f, 6.0f, 2.45f, 1.25f, 0.0f);
            gfx_box(2.45f, -0.05f, 0, 4.5f, 0.1f, 4.5f, RGB(54, 63, 99));
            draw_preview(select_idx, menu_angle, 2.45f);
            gfx_2d();
            gfx_text(220, 44, 1, RGB(175, 195, 230), "SELECTED SHINOBI");
            gfx_text(220, 190, 2, unlocked ? RGB(255, 230, 150) : RGB(255, 100, 100), sd->name);
            if (!unlocked) {
                sprintf(buf, "LOCKED: %s", unlock_hint[select_idx]);
                gfx_text(220, 211, 1, RGB(255, 125, 125), buf);
            } else {
                sprintf(buf, "HP %d", sd->hp);
                gfx_text(220, 211, 1, RGB(215, 225, 245), buf);
                sprintf(buf, "ULT: %s", sd->ult_name);
                gfx_text(220, 224, 1, RGB(255, 205, 100), buf);
                sprintf(buf, "AWAKEN: %s", sd->has_awk ? sd->awk_name : "NONE");
                gfx_text(220, 237, 1, RGB(255, 165, 110), buf);
            }
            if (note_t > 0) gfx_text(220, 251, 1, RGB(255, 120, 120), note);
            gfx_end();
        } break;
        case GS_STORY: {
            char buf[80];
            int mi = row_mission[story_row];
            gfx_begin(RGB(25, 18, 30));
            gfx_2d();
            gfx_gradient(0, 0, 480, 272, RGB(20, 15, 40), RGB(70, 35, 60));
            gfx_text(16, 10, 3, RGB(255, 160, 30), "STORY MODE");
            for (i = 0; i < NUM_ROWS; i++) {
                int m2 = row_mission[i];
                const char *st;
                unsigned col;
                if (m2 < 0) { st = "SOON"; col = RGB(110, 110, 110); }
                else if (!mission_open(m2)) { st = "LOCKED"; col = RGB(170, 100, 100); }
                else if ((sv.completed >> m2) & 1u) { st = "CLEARED"; col = RGB(110, 220, 120); }
                else { st = "NEW"; col = RGB(255, 220, 60); }
                sprintf(buf, "%s%s", i == story_row ? "> " : "  ", row_title[i]);
                gfx_text(14, 46.0f + i * 24, 2, i == story_row ? RGB(255, 255, 255) : RGB(190, 190, 190), buf);
                gfx_text(350, 46.0f + i * 24, 2, col, st);
            }
            if (mi >= 0) {
                const Mission *m = &missions[mi];
                sprintf(buf, "%s VS %s", roster[m->pdef].name, roster[m->edef].name);
                gfx_text(14, 200, 1, RGB(120, 200, 255), buf);
                sprintf(buf, "OBJECTIVE: %s", m->objective);
                gfx_text(14, 212, 1, RGB(255, 220, 60), buf);
                if (m->psup[0] >= 0) {
                    sprintf(buf, "SUPPORT: %s%s%s", roster[m->psup[0]].name, m->psup[1] >= 0 ? " + " : "",
                            m->psup[1] >= 0 ? roster[m->psup[1]].name : "");
                    gfx_text(14, 224, 1, RGB(120, 255, 150), buf);
                }
                if (m->unlock_char >= 0) {
                    sprintf(buf, "REWARD: UNLOCKS %s", roster[m->unlock_char].name);
                    gfx_text(240, 224, 1, RGB(110, 220, 120), buf);
                }
            }
            if (note_t > 0) gfx_text(14, 240, 1, RGB(255, 120, 120), note);
            gfx_text(10, 262, 1, RGB(170, 170, 170), "CROSS: PLAY   SQUARE: ERASE PROGRESS   CIRCLE: BACK");
            gfx_end();
        } break;
        case GS_DIALOGUE: {
            const Mission *m = &missions[cur_mission];
            const Line *L = dlg_post ? m->post : m->pre;
            const Line *cl = &L[dlg_idx];
            char buf[160];
            int len = (int)strlen(cl->txt), shown = dlg_chars < len ? dlg_chars : len;
            Fighter a, b;
            gfx_begin(sky_bot[m->arena]);
            draw_sky(m->arena);
            gfx_3d();
            gfx_camera(0.0f, 1.9f, 6.5f, 0.0f, 1.5f, 0.0f);
            draw_arena(m->arena, 999.0f, 999.0f);
            memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
            a.def = m->pdef; a.x = -1.8f; a.fx = 1.0f; a.fz = 0.0f;
            b.def = m->edef; b.x = 1.8f;  b.fx = -1.0f; b.fz = 0.0f;
            draw_fighter(&a);
            draw_fighter(&b);
            gfx_2d();
            sprintf(buf, "CHAPTER %d: %s", m->chapter, m->title);
            gfx_text(10, 6, 1, RGB(255, 255, 255), buf);
            gfx_rect(8, 150, 464, 112, RGBA(0, 0, 0, 215));
            gfx_text(18, 156, 2, speaker_color(cl->spk), cl->spk);
            memcpy(buf, cl->txt, (size_t)shown);
            buf[shown] = 0;
            gfx_text_wrap(18, 178, 2, RGB(255, 255, 255), buf, 36);
            gfx_text(260, 250, 1, RGB(180, 180, 180), "CROSS: NEXT   START: SKIP");
            gfx_end();
        } break;
        case GS_BATTLE:
            gfx_begin(sky_bot[cur_arena]);
            draw_battle();
            gfx_end();
            break;
        case GS_RESULT:
            gfx_begin(RGB(15, 15, 25));
            gfx_2d();
            gfx_gradient(0, 0, 480, 272, RGB(10, 10, 20), RGB(50, 30, 50));
            {
                static const char *story_opts[3] = { "RETRY", "MISSION LIST", "MAIN MENU" };
                static const char *free_opts[2] = { "RETRY", "MAIN MENU" };
                int cnt = story_mode ? 3 : 2;
                const char *t = player_won ? "VICTORY" : "MISSION FAILED";
                gfx_text(240 - gfx_text_width(t, 3) / 2, 60, 3, player_won ? RGB(255, 220, 60) : RGB(230, 70, 70), t);
                for (i = 0; i < cnt; i++) {
                    char buf[32];
                    sprintf(buf, "%s%s", i == result_sel ? "> " : "  ", story_mode ? story_opts[i] : free_opts[i]);
                    gfx_text(150, 130.0f + i * 30, 2, i == result_sel ? RGB(255, 220, 60) : RGB(200, 200, 200), buf);
                }
            }
            gfx_end();
            break;
        case GS_COMPLETE: {
            const Mission *m = &missions[cur_mission];
            char buf[80];
            int y = 120, j;
            gfx_begin(RGB(15, 25, 20));
            gfx_2d();
            gfx_gradient(0, 0, 480, 272, RGB(10, 25, 20), RGB(40, 70, 50));
            gfx_text(240 - gfx_text_width("MISSION COMPLETE", 3) / 2, 30, 3, RGB(255, 220, 60), "MISSION COMPLETE");
            gfx_text(240 - gfx_text_width(m->title, 2) / 2, 70, 2, RGB(255, 255, 255), m->title);
            if (m->unlock_char >= 0) {
                sprintf(buf, "NEW CHARACTER: %s", roster[m->unlock_char].name);
                gfx_text(40, y, 2, RGB(110, 220, 120), buf); y += 24;
            }
            for (j = 0; j < NUM_MISSIONS; j++)
                if (missions[j].req == cur_mission) {
                    sprintf(buf, "MISSION UNLOCKED: %s", missions[j].title);
                    gfx_text(40, y, 1, RGB(255, 220, 60), buf); y += 14;
                }
            gfx_text(40, y + 10, 1, save_ok ? RGB(120, 220, 255) : RGB(255, 120, 120),
                     save_ok ? "PROGRESS SAVED" : "SAVE FAILED - NO WRITABLE MEMORY STICK PATH");
            gfx_text(150, 240, 1, RGB(180, 180, 180), "CROSS: BACK TO STORY MENU");
            gfx_end();
        } break;
        }
    }
    return 0;
}
