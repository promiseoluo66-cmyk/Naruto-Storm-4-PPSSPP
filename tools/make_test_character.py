#!/usr/bin/env python3
"""Generate an original low-poly Naruto Uzumaki-inspired PSP character.
No game-ripped or third-party assets. +Y is up; the face points +Z.
"""
import os, struct, sys

parts = []
def box(cx, cy, cz, w, h, d, col):
    x0,x1=cx-w/2,cx+w/2; y0,y1=cy-h/2,cy+h/2; z0,z1=cz-d/2,cz+d/2
    q=[
      [(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)],
      [(x1,y0,z0),(x0,y0,z0),(x0,y1,z0),(x1,y1,z0)],
      [(x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(x0,y1,z0)],
      [(x1,y0,z1),(x1,y0,z0),(x1,y1,z0),(x1,y1,z1)],
      [(x0,y1,z1),(x1,y1,z1),(x1,y1,z0),(x0,y1,z0)],
      [(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1)]]
    parts.append((q,col))
def spike(cx,cy,cz,base,height,col,lx=0,lz=0):
    b=base/2
    a=[(cx-b,cy,cz-b),(cx+b,cy,cz-b),(cx+b,cy,cz+b),(cx-b,cy,cz+b)]
    tip=(cx+lx,cy+height,cz+lz)
    parts.append(([ [a[i],a[(i+1)%4],tip,tip] for i in range(4)],col))

SKIN=(0.96,0.73,0.55); HAIR=(1.0,0.72,0.05)
ORANGE=(0.95,0.22,0.035); DARK=(0.055,0.075,0.12)
BLUE=(0.08,0.28,0.78); METAL=(0.68,0.76,0.82)
BLACK=(0.025,0.03,0.045); WHITE=(0.95,0.96,1.0)

# Legs, orange knee panels, and open-toe ninja sandals
for x in (-0.18,0.18):
    box(x,0.43,0,0.27,0.82,0.29,DARK)
    box(x,0.54,0.155,0.275,0.20,0.025,ORANGE)
    box(x,0.075,0.06,0.30,0.14,0.40,DARK)
    box(x,0.075,0.265,0.19,0.09,0.025,SKIN)
# Torso: orange jacket, dark shoulder yoke, blue collar and zipper
box(0,1.25,0,0.72,0.82,0.40,ORANGE)
box(0,1.58,0.015,0.75,0.19,0.415,DARK)
box(0,1.68,0.02,0.36,0.12,0.43,BLUE)
box(0,1.25,0.207,0.035,0.48,0.018,DARK)
box(0,0.86,0,0.74,0.13,0.42,DARK)
# Sleeves, cuffs, hands
for x in (-0.48,0.48):
    box(x,1.31,0,0.22,0.67,0.27,ORANGE)
    box(x,1.03,0.015,0.225,0.12,0.28,DARK)
    box(x,0.87,0.02,0.18,0.20,0.20,SKIN)
# Head and face
box(0,1.98,0,0.51,0.53,0.45,SKIN)
# Ears
for x in (-0.275,0.275): box(x,1.98,0,0.07,0.16,0.13,SKIN)
# Dark forehead band and silver plate
box(0,2.16,0.015,0.56,0.13,0.49,DARK)
box(0,2.16,0.268,0.27,0.105,0.025,METAL)
# Simplified engraved Leaf-style spiral mark on the metal plate
box(-0.025,2.16,0.284,0.09,0.018,0.008,DARK)
box(0.015,2.135,0.284,0.018,0.045,0.008,DARK)
# Hair cap and iconic bright spiky silhouette
box(0,2.28,-0.005,0.55,0.18,0.49,HAIR)
for x,z,h in [(-.23,.05,.27),(-.12,.13,.36),(0,.16,.42),(.13,.12,.36),(.24,.03,.28),(-.22,-.12,.24),(.20,-.13,.25)]:
    spike(x,2.34,z,0.18,h,HAIR, x*0.30, z*0.22)
# Side locks frame the cheeks
for x in (-0.235,0.235): spike(x,2.05,0.16,0.13,0.25,HAIR,x*0.12,0.04)
# Blue eyes, dark pupils, brows, nose and mouth
for x in (-0.115,0.115):
    box(x,2.00,0.232,0.105,0.075,0.018,WHITE)
    box(x,2.00,0.246,0.043,0.065,0.012,BLUE)
    box(x,2.00,0.255,0.018,0.047,0.008,BLACK)
    box(x,2.075,0.235,0.12,0.025,0.018,HAIR)
box(0,1.94,0.238,0.035,0.035,0.025,SKIN)
box(0,1.895,0.238,0.075,0.018,0.015,DARK)
# Three whisker-like cheek marks on each side
for x in (-1,1):
    for y in (1.965,1.925,1.885):
        box(x*0.19,y,0.239,0.105,0.014,0.012,DARK)
# Back collar flap
box(0,1.57,-0.22,0.42,0.22,0.08,DARK)

def write_obj(path):
    # Target reference height: 1.66 m (Shippuden-era Naruto).
    # The source mesh spans 2.76 model units, so scale it to 1.66 units.
    height_scale = 1.66 / 2.76
    lines=["# Original Naruto-inspired low-poly character; target height 1.66m reference; generated, not ripped.","o original_ninja","vt 0 0","vt 1 0","vt 1 1","vt 0 1"]
    faces=[]; vi=1
    for quads,col in parts:
        for quad in quads:
            ids=[]
            for x,y,z in quad:
                lines.append("v %.4f %.4f %.4f %.3f %.3f %.3f"%(x*height_scale,y*height_scale,z*height_scale,*col)); ids.append(vi); vi+=1
            if quad[2]==quad[3]: faces.append("f %d/1 %d/2 %d/3"%tuple(ids[:3]))
            else: faces.append("f %d/1 %d/2 %d/3 %d/4"%tuple(ids))
    with open(path,"w") as f: f.write("\n".join(lines+faces)+"\n")

def write_texture(path,size=64):
    out=bytearray(b"NMTX")+bytearray(struct.pack("<IIII",1,size,size,1))
    for y in range(size):
        for x in range(size):
            v=0.91+0.06*((x//2+y//2)%2)
            r=int(v*31+0.5); g=min(63,r*2+(1 if r>15 else 0))
            out+=struct.pack("<H",r|(g<<5)|(r<<11))
    with open(path,"wb") as f: f.write(out)

if __name__=="__main__":
    out_dir=sys.argv[1] if len(sys.argv)>1 else "assets/chr/test"
    os.makedirs(out_dir,exist_ok=True)
    write_obj(os.path.join(out_dir,"test_character.obj"))
    write_texture(os.path.join(out_dir,"test_texture.nmtx"))
    print("wrote Naruto-inspired character to",out_dir)
