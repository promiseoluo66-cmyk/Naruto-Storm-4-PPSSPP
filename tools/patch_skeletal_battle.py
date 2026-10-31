from pathlib import Path
p=Path("src/main.c")
s=p.read_text()
def rep(a,b):
 global s
 if a not in s: raise SystemExit("anchor missing: "+a[:80])
 s=s.replace(a,b,1)
rep('#include "storm_anim.h"','#include "storm_anim.h"\n#include "skinned.h"')
rep('static Fighter pl, en;','static Fighter pl, en;\nstatic SkinnedMesh naruto_skin;\nstatic SkinnedInstance naruto_skin_player, naruto_skin_dummy;\nstatic int naruto_skin_ready = 0;')
a='    const Move *mv = (f->state == FS_JUTSU) ? &d->jut[f->move] : NULL;'
b=a+'''
    if (id == 0 && naruto_skin_ready && !f->dead) {
        SkinPose pose;
        SkinnedInstance *instance = (f == &pl) ? &naruto_skin_player : &naruto_skin_dummy;
        memset(&pose, 0, sizeof(pose));
        pose.combo=f->combo; pose.timer=f->timer; pose.combo_dir=f->combo_dir; pose.phase=f->phase;
        if (f->state==FS_ATTACK) pose.action=SKIN_ACTION_ATTACK;
        else if (f->state==FS_JUTSU || f->state==FS_ULT || f->state==FS_TEAM) pose.action=SKIN_ACTION_JUTSU;
        else if (f->y>0.05f) pose.action=SKIN_ACTION_JUMP;
        else if (f->guarding) pose.action=SKIN_ACTION_GUARD;
        else if (f->state==FS_DASH) pose.action=SKIN_ACTION_DASH;
        else if (f->moving) pose.action=SKIN_ACTION_RUN;
        else pose.action=SKIN_ACTION_IDLE;
        gfx_box(f->x,0.025f,f->z,1.25f,0.02f,1.25f,RGB(20,20,20));
        skinned_draw(&naruto_skin,instance,f->x,f->y,f->z,atan2f(f->fx,f->fz),1.45f,&pose);
        return;
    }
'''
rep(a,b)
init = "\n".join([
    "    gfx_init();",
    '    if (skinned_mesh_load(&naruto_skin,"assets/chr/test/naruto_skin.nskl","assets/chr/test/naruto_skin.nmtx")==0) {',
    "        if (skinned_instance_init(&naruto_skin_player,&naruto_skin)==0 &&",
    "            skinned_instance_init(&naruto_skin_dummy,&naruto_skin)==0) naruto_skin_ready=1;",
    "        else { skinned_instance_free(&naruto_skin_player); skinned_instance_free(&naruto_skin_dummy); skinned_mesh_free(&naruto_skin); }",
    "    }",
    "    sceCtrlSetSamplingCycle(0);"
])
rep('    gfx_init();\n    sceCtrlSetSamplingCycle(0);', init)
p.write_text(s)
print("SKELETAL MAIN INTEGRATION APPLIED")
