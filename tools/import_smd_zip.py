#!/usr/bin/env python3
"""Convert the rigged Naruto SMD in the supplied nested ZIP into PSP NRIG/NMTX."""
import io, math, os, posixpath, struct, sys, zipfile
from PIL import Image

SIZE, TILE, MAX_VERTICES, MAX_BONES = 512, 256, 120000, 160
# SMD Z-up to the game's Y-up coordinates: (x,y,z) -> (x,z,-y).
C = ((1.,0.,0.), (0.,0.,1.), (0.,-1.,0.))

def mm(a,b):
    return tuple(tuple(sum(a[r][k]*b[k][c] for k in range(3)) for c in range(3)) for r in range(3))

def mt(a):
    return tuple(tuple(a[c][r] for c in range(3)) for r in range(3))

def local_matrix(tx,ty,tz,rx,ry,rz):
    sx,cx=math.sin(rx),math.cos(rx); sy,cy=math.sin(ry),math.cos(ry); sz,cz=math.sin(rz),math.cos(rz)
    ax=((1.,0.,0.),(0.,cx,-sx),(0.,sx,cx))
    ay=((cy,0.,sy),(0.,1.,0.),(-sy,0.,cy))
    az=((cz,-sz,0.),(sz,cz,0.),(0.,0.,1.))
    rot=mm(C,mm(mm(az,ay),mm(ax,mt(C))))
    return (rot[0][0],rot[0][1],rot[0][2],tx,
            rot[1][0],rot[1][1],rot[1][2],tz,
            rot[2][0],rot[2][1],rot[2][2],-ty)

def get_texture(names, material):
    base=posixpath.basename(material.replace('\\\\','/'))
    if base.lower().endswith('.png.001') or base.lower().endswith('.png.002'):
        base=base.rsplit('.',1)[0]
    matches=[n for n in names if posixpath.basename(n.replace('\\\\','/')).lower()==base.lower()]
    if matches: return matches[0]
    # The supplied rip omits nrta_tex2.png as a separate file; map its slot to the
    # Naruto diffuse sheet rather than aborting the whole import.
    if base.lower()=='nrta_tex2.png':
        matches=[n for n in names if posixpath.basename(n.replace('\\\\','/')).lower()=='nrt_tex02.png']
        if matches:
            print('TEXTURE FALLBACK',material,'->',matches[0]); return matches[0]
    raise ValueError('Texture not found for material: '+material)

def parse_smd(raw):
    lines=raw.decode('utf-8','replace').splitlines()
    ni=lines.index('nodes')+1; ne=lines.index('end',ni)
    si=lines.index('skeleton')+1; se=lines.index('end',si)
    ti=lines.index('triangles')+1; te=lines.index('end',ti)
    bnames=[]; parents=[]
    for line in lines[ni:ne]:
        p=line.split()
        if len(p)>=3:
            if int(p[0])!=len(bnames): raise ValueError('Nonsequential SMD bone IDs')
            bnames.append(p[1].strip('"')); parents.append(int(p[2]))
    if not bnames or len(bnames)>MAX_BONES: raise ValueError('Invalid bone count')
    binds=[None]*len(bnames)
    reading=False
    for line in lines[si:se]:
        p=line.split()
        if not p: continue
        if p[0]=='time':
            if p[1]=='0': reading=True
            elif reading: break
            continue
        if reading and len(p)>=7 and p[0].isdigit():
            i=int(p[0]); binds[i]=local_matrix(*map(float,p[1:7]))
    if any(b is None for b in binds): raise ValueError('SMD reference pose incomplete')
    if any(parent>=i or parent < -1 for i,parent in enumerate(parents)):
        raise ValueError('SMD hierarchy is not parent-before-child')
    records=[]; materials=[]; active=None
    for line in lines[ti:te]:
        p=line.split()
        if not p: continue
        if len(p)==1:
            active=p[0]
            if active not in materials: materials.append(active)
            continue
        if len(p)<12: raise ValueError('Malformed SMD vertex')
        x,y,z=map(float,p[1:4]); u,v=float(p[7]),float(p[8]); n=int(p[9])
        if n<1: raise ValueError('Unweighted SMD vertex')
        inf=[]
        for k in range(n):
            q=10+2*k
            if q+1>=len(p): raise ValueError('Truncated weights')
            bid,w=int(p[q]),float(p[q+1])
            if bid<0 or bid>=len(bnames) or w<0: raise ValueError('Invalid skin weight')
            inf.append((bid,w))
        inf.sort(key=lambda x:x[1],reverse=True); inf=inf[:3]
        total=sum(w for _,w in inf)
        if total<=0: raise ValueError('Weights sum to zero')
        inf=[(b,w/total) for b,w in inf]
        while len(inf)<3: inf.append((inf[0][0],0.))
        records.append((active,(x,z,-y,u,1.-v,inf)))
    if not records or len(records)%3 or len(records)>MAX_VERTICES:
        raise ValueError('Invalid SMD triangle vertex count')
    return bnames,parents,binds,materials,records

def run(archive,prefix):
    with zipfile.ZipFile(archive) as outer:
        nested=next((n for n in outer.namelist() if n.lower().endswith('.zip')),None)
        if not nested: raise ValueError('No nested model ZIP')
        with zipfile.ZipFile(io.BytesIO(outer.read(nested))) as z:
            names=z.namelist()
            smd=next((n for n in names if n.replace('\\\\','/').lower().endswith('/naruto.smd')),None)
            if not smd: raise ValueError('Naruto.smd not found')
            bnames,parents,binds,materials,records=parse_smd(z.read(smd))
            mat_path={m:get_texture(names,m) for m in materials}
            paths=[]
            for m in materials:
                if mat_path[m] not in paths: paths.append(mat_path[m])
            if len(paths)>4: raise ValueError('Texture atlas supports at most four images')
            atlas=Image.new('RGBA',(SIZE,SIZE),(0,0,0,0)); info={}
            resample=getattr(Image,'Resampling',Image).LANCZOS
            for slot,path in enumerate(paths):
                with Image.open(io.BytesIO(z.read(path))) as src: im=src.convert('RGBA')
                im.thumbnail((TILE-4,TILE-4),resample)
                ox,oy=(slot%2)*TILE,(slot//2)*TILE
                x,y=ox+(TILE-im.width)//2,oy+(TILE-im.height)//2
                atlas.paste(im,(x,y)); info[path]=(x,y,im.width,im.height)
                print('ATLAS',slot,path,im.width,im.height)
            out=bytearray(struct.pack('<4sIII',b'NRIG',1,len(records),len(bnames)))
            for material,vtx in records:
                x,y,z,u,v,inf=vtx; x0,y0,w,h=info[mat_path[material]]
                au=(x0+0.5+u*max(0,w-1))/SIZE
                av=(y0+0.5+v*max(0,h-1))/SIZE
                b0,b1,b2=(inf[0][0],inf[1][0],inf[2][0])
                w0,w1,w2=(inf[0][1],inf[1][1],inf[2][1])
                out += struct.pack('<ffIfffHHHfff',au,av,0xffffffff,x,y,z,b0,b1,b2,w0,w1,w2)
            for parent,matrix in zip(parents,binds): out += struct.pack('<i12f',parent,*matrix)
            os.makedirs(os.path.dirname(prefix) or '.',exist_ok=True)
            with open(prefix+'.nrig','wb') as f: f.write(out)
            tex=struct.pack('<4sIIII',b'NMTX',1,SIZE,SIZE,3)+atlas.tobytes()
            with open(prefix+'.nmtx','wb') as f: f.write(tex)
            print('IMPORTED',smd,'bones',len(bnames),'vertices',len(records),'triangles',len(records)//3)
            print('RIG BONES',[(i,n) for i,n in enumerate(bnames) if n in ('hip','L_hleg','L_lleg','R_hleg','R_lleg','L_harm','L_larm','R_harm','R_larm')])
            print('OUTPUT',prefix+'.nrig',len(out),'bytes; '+prefix+'.nmtx',len(tex),'bytes')

if __name__=='__main__':
    if len(sys.argv)!=3: raise SystemExit('Usage: import_smd_zip.py source.zip output_prefix')
    try: run(sys.argv[1],sys.argv[2])
    except Exception as e: raise SystemExit('SMD import failed: '+str(e))
