from inspect_port import *
from port_config import GAME
game=GAME/'Data';stage=ROOT/'staged'
def bsa_names(path):
 with path.open('rb') as f:
  magic,v,off,flags,nfolders,nfiles,folderlen,filelen,ft=struct.unpack('<4s8I',f.read(36));assert magic==b'BSA\0'
  f.seek(off);counts=[]
  for _ in range(nfolders):
   b=f.read(24 if v==105 else 16);counts.append(struct.unpack_from('<I',b,8)[0])
  folders=[]
  for count in counts:
   n=f.read(1)[0];folder=f.read(n).rstrip(b'\0').decode();f.seek(count*16,1);folders.extend([folder]*count)
  names=f.read(filelen).split(b'\0');assert len(folders)==nfiles
  return [(a+'\\'+b.decode()).lower().replace('/','\\') for a,b in zip(folders,names)]
available={str(p.relative_to(stage)).lower() for p in stage.rglob('*') if p.is_file()}
available.update(str(p.relative_to(game)).lower() for p in game.rglob('*') if p.is_file())
for p in game.glob('*.bsa'):available.update(bsa_names(p))
available.update(bsa_names(stage/'SkyUI_SE.bsa'))
missing=collections.defaultdict(set)
for line in (stage/'mesh-textures.tsv').read_text().splitlines():
 mesh,tex=line.split('\t',1);tex=tex.lower().replace('/','\\')
 if tex not in available:missing[tex].add(mesh)
missing_models=[]
for typ,fid,ver,body in records((stage/'SkyScape.esm').read_bytes()):
 for t,v in subs(body):
  if t in ('MODL','MOD2','MOD3','MOD4','MOD5') and v.lower().rstrip(b'\0').endswith(b'.nif'):
   model=v.rstrip(b'\0').decode(errors='replace').replace('/','\\').lower()
   if not model.startswith('meshes\\'):model='meshes\\'+model
   if model not in available:missing_models.append((typ,hex(fid),model))
report={'missing_textures':{k:sorted(v) for k,v in missing.items()},'missing_models':missing_models}
(ROOT/'asset-audit.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
