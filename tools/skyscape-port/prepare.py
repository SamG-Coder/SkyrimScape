from inspect_port import *
import shutil,hashlib
source=ROOT/'original'; stage=ROOT/'staged'; stage.mkdir(parents=True,exist_ok=True); changes=collections.Counter(); excluded=[]
def pack_sub(t,v):
 b=t.encode()
 return (b'XXXX'+struct.pack('<HI',4,len(v))+b+bytes(2)+v) if len(v)>65535 else b+struct.pack('<H',len(v))+v
def convert_block(data,start=0,end=None):
 end=len(data) if end is None else end;out=bytearray()
 while start<end:
  head=bytearray(data[start:start+24]);typ=head[:4].decode();size=struct.unpack_from('<I',head,4)[0]
  if typ=='GRUP':
   body=convert_block(data,start+24,start+size);struct.pack_into('<I',head,4,len(body)+24);start+=size
  else:
   flags=struct.unpack_from('<I',head,8)[0];body=data[start+24:start+24+size];start+=24+size
   if flags&0x40000:body=zlib.decompress(body[4:])
   fields=list(subs(body));new=[]
   for tag,v in fields:
    if typ=='WEAP' and tag=='CRDT':
     assert len(v)==16
     v=v[:12]+bytes(4)+v[12:16]+bytes(4);changes['weapon_critical_layout']+=1
    if typ=='STAT' and tag=='DNAM':
     assert len(v)==8;v+=bytes(4);changes['static_snow_defaults']+=1
    if typ=='AMMO' and tag=='DATA':
     assert len(v)==16;v+=struct.pack('<f',0);changes['ammo_weight_defaults']+=1
    if typ=='WATR' and tag=='DNAM':
     assert len(v)==228,(typ,tag,len(v));v+=struct.pack('<f',1);changes['water_flowmap_scale']+=1
    if typ=='WATR' and tag=='FNAM':
     v=bytes([v[0]&~24]);changes['water_disable_new_flags']+=1
    if typ=='TES4' and tag=='HEDR':v=struct.pack('<f',1.71)+v[4:]
    if tag in ('MODL','MOD2','MOD3','MOD4','MOD5'):
     fixed={b'runesky\\armor\\lumshield01go.nif\0':b'clutter\\runesky\\shield\\lumshield01go.nif\0',b'effects\\dungeons\\imperial\\clutterkits\\impstoneblock01.nif\0':b'dungeons\\imperial\\clutterkits\\impstoneblock01.nif\0'}.get(v.lower())
     if fixed:v=fixed;changes['repaired_model_paths']+=1
    new.append((tag,v))
   if typ=='LTEX' and not any(t=='INAM' for t,v in new):new.append(('INAM',bytes(4)));changes['land_texture_snow_defaults']+=1
   if typ=='WATR' and not any(t=='NAM5' for t,v in new):new.append(('NAM5',b'\0'));changes['water_flow_texture']+=1
   body=b''.join(pack_sub(t,v) for t,v in new)
   if flags&0x40000:body=struct.pack('<I',len(body))+zlib.compress(body)
   struct.pack_into('<I',head,4,len(body));struct.pack_into('<H',head,20,44)
  out+=head+body
 assert start==end
 return out
orig=list(records((source/'SkyScape.esm').read_bytes()))
result=convert_block((source/'SkyScape.esm').read_bytes());ported=list(records(result))
assert len(orig)==len(ported)
for old,new in zip(orig,ported):
 assert old[:2]==new[:2] and new[2]==44
 a=list(subs(old[3]));b=list(subs(new[3]))
 allowed={'WEAP':{'CRDT'},'STAT':{'DNAM'},'AMMO':{'DATA'},'WATR':{'DNAM','FNAM','NAM5'},'LTEX':{'INAM'},'TES4':{'HEDR'},'ARMO':{'MOD2','MOD4'},'PROJ':{'MODL'}}.get(old[0],set())
 assert [(t,v) for t,v in a if t not in allowed]==[(t,v) for t,v in b if t not in allowed]
 if old[0]=='WEAP':
  a=dict(a);b=dict(b)
  if 'CRDT' in a:assert a['CRDT'][12:16]==b['CRDT'][16:20] and a['CRDT'][:9]==b['CRDT'][:9]
(stage/'SkyScape.esm').write_bytes(result)
texture_formats=collections.Counter();texture_issues=[]
for p in source.rglob('*'):
 if not p.is_file():continue
 rel=p.relative_to(source);ext=p.suffix.lower()
 if ext in ('.esm','.nif'):continue
 if rel.parts[0].lower()=='scripts':
  if ext!='.pex' or not p.name.lower().startswith(('rs','qf_rs','pf_rs','sf_rs','tif__','tis')):
   excluded.append(str(rel));continue
 elif ext not in ('.dds','.xwm','.wav','.lip','.seq'):continue
 if ext=='.dds':
  d=p.read_bytes();assert d[:4]==b'DDS ' and len(d)>=128
  h,w=struct.unpack_from('<II',d,12);flags,fourcc,bits=struct.unpack_from('<I4sI',d,80)
  texture_formats[str((flags,fourcc.decode('ascii',errors='replace'),bits))]+=1
  if not w or not h or (fourcc not in (b'DXT1',b'DXT3',b'DXT5',b'DX10',bytes(4))):texture_issues.append(str(rel))
 dest=stage/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dest)
for name in ('SkyUI_SE.esp','SkyUI_SE.bsa'):shutil.copy2(ROOT/'skyui'/name,stage/name)
report={'record_count':len(ported),'record_conversion':dict(changes),'textures':dict(texture_formats),'texture_issues':texture_issues,'scripts':[p.name for p in (stage/'scripts').glob('*.pex')],'excluded_scripts':excluded,'method':'nifly SSE mesh conversion; schema-based ESM conversion against xEdit definitions, not a Creation Kit resave'}
(ROOT/'conversion-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps({k:v for k,v in report.items() if k not in ('scripts','excluded_scripts')},indent=2));print('Scripts staged',len(report['scripts']),'excluded',len(excluded))
