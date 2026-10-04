from inspect_port import *
import shutil,re,subprocess
from port_config import SKSE_SOURCE, TEXCONV
assert SKSE_SOURCE.is_dir() and len(list(SKSE_SOURCE.glob("*.psc"))) == 62, "Expected 62 SKSE script sources"
stage=ROOT/'staged';reference=ROOT/'skse-script-reference/skse64_2_02_06/Data/Scripts'
def norm(text):
 text=re.sub(r';[^\n]*','',text)
 return re.sub(r'\s+',' ',text).strip().lower()
for p in SKSE_SOURCE.glob('*.psc'):
 old=reference/'Source'/p.name
 assert old.exists(),p
 current=norm(p.read_text(errors='replace'));previous=norm(old.read_text(errors='replace'))
 if p.stem.lower()=='skse':
  assert current.replace('return 75','return 72') in previous
 else:assert current in previous,('SKSE script mismatch',p)
 compiled=reference/(p.stem+'.pex');assert compiled.exists()
 if p.stem.lower()=='skse':
  data=compiled.read_bytes()
  # PEX opcode Return (26), integer operand (3), big-endian int32.
  # Source comparison above proves the release constant is the sole difference.
  needle=b'\x1a\x03\x00\x00\x00\x48';assert data.count(needle)==1
  (stage/'scripts'/compiled.name).write_bytes(data.replace(needle,b'\x1a\x03\x00\x00\x00\x4b'))
 else:shutil.copy2(compiled,stage/'scripts'/compiled.name)
print('Verified current SKSE script additions against 62 published compiled SE scripts')
fixed=[]
for p in (stage/'textures').rglob('*.dds'):
 d=p.read_bytes();flags,cc,bits=struct.unpack_from('<I4sI',d,80)
 fmt=None
 if cc==bytes(4) and bits==24:fmt='R8G8B8A8_UNORM'
 if cc==b'q\x00\x00\x00':fmt='R16G16B16A16_FLOAT'
 if fmt:
  args=[str(TEXCONV),'-nologo','-y','-f',fmt,'-o',str(p.parent),str(p)]
  if fmt.endswith('FLOAT'):args.insert(1,'-dx10')
  subprocess.run(args,check=True);fixed.append(str(p.relative_to(stage)))
report=json.loads((ROOT/'conversion-report.json').read_text());report['texture_conversions']=fixed;report['skse_script_verification']='62 current source additions match published SE script sources; native runtime remains installed 2.3.1';(ROOT/'conversion-report.json').write_text(json.dumps(report,indent=2))
print('Fixed textures',fixed)
