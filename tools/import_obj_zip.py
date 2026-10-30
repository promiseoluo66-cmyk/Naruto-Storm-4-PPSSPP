#!/usr/bin/env python3
"""Import an OBJ/MTL/PNG character from a nested ZIP into PSP NMDL + NMTX.
Requires Pillow. Combines up to four material textures into a 512x512 RGBA atlas.
"""
import io, os, posixpath, struct, sys, zipfile
from PIL import Image

SIZE, TILE, LIMIT = 512, 256, 120000

def find(names, suffix):
    for n in names:
        if n.replace("\\", "/").lower().endswith(suffix.lower()):
            return n
    raise ValueError("Missing archive entry: " + suffix)

def idx(raw, count):
    n = int(raw)
    n = n - 1 if n > 0 else count + n
    if n < 0 or n >= count:
        raise ValueError("OBJ index outside array: " + raw)
    return n

def run(archive, prefix):
    with zipfile.ZipFile(archive) as outer:
        nested = next((n for n in outer.namelist() if n.lower().endswith(".zip")), None)
        if not nested:
            raise ValueError("Outer ZIP has no nested ZIP")
        with zipfile.ZipFile(io.BytesIO(outer.read(nested))) as z:
            names = z.namelist()
            obj = find(names, "/Naruto.obj")
            mtl = find(names, "/Naruto.mtl")
            mats, current = {}, None
            for line in z.read(mtl).decode("utf-8", "replace").splitlines():
                p = line.strip().split()
                if not p: continue
                if p[0] == "newmtl" and len(p) > 1:
                    current = p[1]
                elif p[0] == "map_Kd" and current:
                    ref = p[-1].replace("\\", "/")
                    target = posixpath.normpath(posixpath.join(posixpath.dirname(obj), ref))
                    found = next((n for n in names if n.replace("\\", "/").lower() == target.lower()), None)
                    if found is None:
                        found = next((n for n in names if posixpath.basename(n.replace("\\", "/")).lower() == posixpath.basename(ref).lower()), None)
                    if found is None: raise ValueError("Texture missing: " + ref)
                    mats[current] = found
            pos, uv, faces, used = [], [], [], []
            active = None
            for line in z.read(obj).decode("utf-8", "replace").splitlines():
                p = line.split("#", 1)[0].split()
                if not p: continue
                if p[0] == "v": pos.append(tuple(map(float, p[1:4])))
                elif p[0] == "vt": uv.append((float(p[1]), float(p[2]) if len(p)>2 else 0.0))
                elif p[0] == "usemtl" and len(p)>1:
                    active = p[1]
                    if active not in used: used.append(active)
                elif p[0] == "f":
                    if active not in mats: raise ValueError("Face has unmapped material: " + str(active))
                    refs=[]
                    for token in p[1:]:
                        b=token.split("/")
                        if len(b)<2 or not b[1]: raise ValueError("Face has no UV: "+token)
                        refs.append((idx(b[0],len(pos)),idx(b[1],len(uv))))
                    for k in range(1,len(refs)-1):
                        faces.append((active,(refs[0],refs[k],refs[k+1])))
            if not faces: raise ValueError("OBJ contains no faces")
            count=len(faces)*3
            if count>LIMIT: raise ValueError("Vertex count exceeds PSP importer limit")
            paths=[]
            for m in used:
                if mats[m] not in paths: paths.append(mats[m])
            if len(paths)>4: raise ValueError("More than four unique textures; atlas limit is four")
            atlas=Image.new("RGBA",(SIZE,SIZE),(0,0,0,0))
            info={}
            resample=getattr(Image,"Resampling",Image).LANCZOS
            for slot,path in enumerate(paths):
                with Image.open(io.BytesIO(z.read(path))) as source: im=source.convert("RGBA")
                im.thumbnail((TILE-4,TILE-4),resample)
                tx=(slot%2)*TILE; ty=(slot//2)*TILE
                x=tx+(TILE-im.width)//2; y=ty+(TILE-im.height)//2
                atlas.paste(im,(x,y)); info[path]=(x,y,im.width,im.height)
                print("ATLAS",slot,path,im.width,im.height)
            data=bytearray(struct.pack("<4sII",b"NMDL",2,count))
            for material,tri in faces:
                x0,y0,w,h=info[mats[material]]
                for vi,ti in tri:
                    x,y,zv=pos[vi]; u,v=uv[ti]
                    au=(x0+0.5+u*max(0,w-1))/SIZE
                    av=(y0+0.5+(1-v)*max(0,h-1))/SIZE
                    data += struct.pack("<ffIfff",au,av,0xFFFFFFFF,x,y,zv)
            os.makedirs(os.path.dirname(prefix) or ".",exist_ok=True)
            with open(prefix+".nmdl","wb") as f: f.write(data)
            tex=struct.pack("<4sIIII",b"NMTX",1,SIZE,SIZE,3)+atlas.tobytes()
            with open(prefix+".nmtx","wb") as f: f.write(tex)
            print("IMPORTED",obj,"positions",len(pos),"uvs",len(uv),"triangles",len(faces),"vertices",count)
            print("MATERIALS",len(used),"UNIQUE_TEXTURES",len(paths))
            print("OUTPUT",prefix+".nmdl",len(data),"bytes")
            print("OUTPUT",prefix+".nmtx",len(tex),"bytes")

if __name__ == "__main__":
    if len(sys.argv)!=3: raise SystemExit("Usage: python3 tools/import_obj_zip.py source.zip output_prefix")
    try: run(sys.argv[1],sys.argv[2])
    except Exception as e: raise SystemExit("Import failed: "+str(e))
