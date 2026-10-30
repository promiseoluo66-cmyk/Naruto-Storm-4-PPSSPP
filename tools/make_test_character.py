#!/usr/bin/env python3
"""Generate an original, smoother Naruto-inspired low-poly character for PSP.
+Y is up and the face points +Z. Geometry is procedural; no ripped game assets.
"""
import os, struct, sys, math

parts = []
SKIN=(0.96,0.70,0.50); HAIR=(1.0,0.68,0.035)
ORANGE=(0.92,0.19,0.025); ORANGE_LIGHT=(1.0,0.30,0.045)
DARK=(0.035,0.045,0.075); BLUE=(0.055,0.24,0.78)
METAL=(0.72,0.79,0.84); BLACK=(0.018,0.022,0.03)
WHITE=(0.96,0.98,1.0); GOLD=(1.0,0.78,0.12)

def ellipsoid(cx,cy,cz,rx,ry,rz,col,segments=12,rings=8):
    verts=[]
    for j in range(rings+1):
        lat=-math.pi/2+math.pi*j/rings
        for i in range(segments):
            lon=2*math.pi*i/segments
            verts.append((cx+rx*math.cos(lat)*math.cos(lon),
                          cy+ry*math.sin(lat),
                          cz+rz*math.cos(lat)*math.sin(lon)))
    quads=[]
    for j in range(rings):
        for i in range(segments):
            a=j*segments+i; b=j*segments+(i+1)%segments
            quads.append([verts[a],verts[b],verts[b+segments],verts[a+segments]])
    parts.append((quads,col))

def tapered_limb(a,b,r1,r2,col,segments=9):
    ax,ay,az=a; bx,by,bz=b
    dx,dy,dz=bx-ax,by-ay,bz-az
    length=math.sqrt(dx*dx+dy*dy+dz*dz) or 1
    ux,uy,uz=dx/length,dy/length,dz/length
    # Build a stable pair of perpendicular basis vectors.
    px,py,pz=-uy,ux,0
    plen=math.sqrt(px*px+py*py+pz*pz)
    if plen<0.01: px,py,pz=1,0,0; plen=1
    px,py,pz=px/plen,py/plen,pz/plen
    qx,qy,qz=uy*pz-uz*py,uz*px-ux*pz,ux*py-uy*px
    ring1=[]; ring2=[]
    for i in range(segments):
        t=2*math.pi*i/segments
        c,s=math.cos(t),math.sin(t)
        ring1.append((ax+r1*(px*c+qx*s),ay+r1*(py*c+qy*s),az+r1*(pz*c+qz*s)))
        ring2.append((bx+r2*(px*c+qx*s),by+r2*(py*c+qy*s),bz+r2*(pz*c+qz*s)))
    quads=[]
    for i in range(segments):
        n=(i+1)%segments
        quads.append([ring1[i],ring1[n],ring2[n],ring2[i]])
    parts.append((quads,col))
    ellipsoid(ax,ay,az,r1,r1,r1,col,segments,5)
    ellipsoid(bx,by,bz,r2,r2,r2,col,segments,5)

def spike(cx,cy,cz,base,height,col,dx=0,dz=0):
    b=base/2
    ring=[(cx-b,cy,cz-b),(cx+b,cy,cz-b),(cx+b,cy,cz+b),(cx-b,cy,cz+b)]
    tip=(cx+dx,cy+height,cz+dz)
    parts.append(([[ring[i],ring[(i+1)%4],tip] for i in range(4)],col))

# Feet and articulated-looking legs
for x in (-0.17,0.17):
    tapered_limb((x,0.20,0),(x,0.82,0),0.105,0.13,DARK)
    ellipsoid(x,0.095,0.105,0.14,0.075,0.23,DARK)
    ellipsoid(x,0.085,0.255,0.075,0.045,0.035,SKIN,10,6)
    ellipsoid(x,0.56,0.075,0.115,0.13,0.07,ORANGE)
    ellipsoid(x,0.39,0.005,0.12,0.11,0.12,DARK)
# Torso: shaped jacket, shoulders and collar
ellipsoid(0,1.27,0,0.34,0.43,0.205,ORANGE,16,10)
ellipsoid(0,1.55,0,0.37,0.16,0.22,DARK,14,8)
ellipsoid(0,1.62,0.04,0.19,0.12,0.22,BLUE,12,8)
ellipsoid(0,1.20,0.205,0.025,0.29,0.018,DARK,8,6)
# Jacket panels and hem
for x in (-1,1):
    ellipsoid(x*0.19,1.25,0.17,0.12,0.32,0.055,ORANGE_LIGHT)
    ellipsoid(x*0.29,1.49,0,0.16,0.15,0.18,ORANGE)
    tapered_limb((x*0.30,1.48,0),(x*0.48,1.10,0.015),0.105,0.075,ORANGE)
    ellipsoid(x*0.48,1.08,0.02,0.09,0.075,0.09,DARK)
    ellipsoid(x*0.49,0.91,0.045,0.075,0.11,0.075,SKIN)
ellipsoid(0,0.91,0,0.31,0.10,0.19,DARK)
# Neck and head
ellipsoid(0,1.76,0.005,0.105,0.15,0.105,SKIN)
ellipsoid(0,2.02,0.015,0.245,0.30,0.205,SKIN,16,12)
for x in (-1,1):
    ellipsoid(x*0.25,2.02,0,0.055,0.09,0.07,SKIN,10,7)
# Forehead protector: dark wrap, metallic plate and simple engraved motif
ellipsoid(0,2.235,0.015,0.27,0.065,0.215,DARK,16,8)
ellipsoid(0,2.235,0.207,0.125,0.055,0.025,METAL,12,8)
ellipsoid(-0.025,2.235,0.233,0.055,0.012,0.008,DARK,8,5)
ellipsoid(0.018,2.215,0.234,0.012,0.025,0.008,DARK,8,5)
# Hair cap and distinct swept spikes
ellipsoid(0,2.285,-0.005,0.265,0.13,0.215,HAIR,16,9)
for x,z,h,dx,dz in [(-.22,.03,.24,-.08,.02),(-.14,.12,.34,-.04,.03),(-.045,.17,.39,0,.04),(.07,.16,.37,.03,.04),(.17,.10,.31,.07,.03),(.23,.015,.23,.09,0),(-.20,-.10,.22,-.06,-.03),(.18,-.12,.24,.06,-.04)]:
    spike(x,2.32,z,0.17,h,HAIR,dx,dz)
for x in (-1,1):
    spike(x*0.205,2.08,0.12,0.12,0.22,HAIR,x*0.04,0.03)
# Eyes, pupils, brows, nose, mouth
for x in (-1,1):
    ellipsoid(x*0.092,2.025,0.193,0.062,0.042,0.025,WHITE,12,7)
    ellipsoid(x*0.092,2.025,0.216,0.021,0.035,0.012,BLUE,10,7)
    ellipsoid(x*0.092,2.025,0.227,0.009,0.026,0.007,BLACK,8,6)
    ellipsoid(x*0.092,2.083,0.19,0.071,0.018,0.025,HAIR,10,6)
ellipsoid(0,1.972,0.214,0.025,0.032,0.025,SKIN,10,6)
ellipsoid(0,1.925,0.211,0.045,0.009,0.009,DARK,10,5)
# Three whisker marks on each cheek
for x in (-1,1):
    for y in (2.005,1.97,1.935):
        ellipsoid(x*0.16,y,0.197,0.052,0.006,0.008,DARK,8,5)
# Back of jacket collar
ellipsoid(0,1.58,-0.17,0.22,0.12,0.06,DARK,12,7)

def write_obj(path):
    # Keep the character at the existing 1.66-unit reference height.
    height_scale=1.66/2.76
    lines=["# Original smooth low-poly anime ninja; procedural geometry; not ripped.","o original_ninja","vt 0 0","vt 1 0","vt 1 1","vt 0 1"]
    faces=[]; vi=1
    for quads,col in parts:
        for quad in quads:
            ids=[]
            for x,y,z in quad:
                lines.append("v %.5f %.5f %.5f %.3f %.3f %.3f"%(x*height_scale,y*height_scale,z*height_scale,*col))
                ids.append(vi); vi+=1
            if len(ids)==3: faces.append("f %d/1 %d/2 %d/3"%tuple(ids))
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
    print("wrote smoother original ninja character to",out_dir)
