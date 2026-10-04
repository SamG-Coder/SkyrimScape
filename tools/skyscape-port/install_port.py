from pathlib import Path
import json,hashlib,shutil,datetime,configparser
from port_config import ROOT, GAME, LOCAL, DOCUMENTS, BACKUPS, require_game_closed
require_game_closed()
root=ROOT;stage=root/'staged';game=GAME;data=game/'Data'
assert (stage/'SkyScape.esm').is_file() and (stage/'SkyUI_SE.bsa').is_file()
audit=json.loads((root/'asset-audit.json').read_text())
assert not audit['missing_textures'] and not audit['missing_models'], 'Resolve missing assets before installing'
backup=BACKUPS/('skyscape-se-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));backup.mkdir(parents=True)
manifest=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(src,dst):
 existing=dst.exists(); entry={'destination':str(dst),'existed':existing,'backup':None,'sha256':None}
 if existing:
  b=backup/'previous'/str(len(manifest));b.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(dst,b);entry['backup']=str(b)
 manifest.append(entry);(backup/'manifest.json').write_text(json.dumps(manifest,indent=2))
 dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst);entry['sha256']=sha(src);assert sha(dst)==entry['sha256']
 (backup/'manifest.json').write_text(json.dumps(manifest,indent=2))
plugin=data/'SKSE/Plugins/SkyrimScape.dll';before=sha(plugin)
for src in sorted(stage.rglob('*')):
 if src.is_file() and src.suffix.lower() in ('.nif','.dds','.pex','.xwm','.wav','.lip','.seq','.esm','.esp','.bsa'):
  save(src,data/src.relative_to(stage))
local=LOCAL
for filename,entries in [('plugins.txt',['*SkyScape.esm','*SkyUI_SE.esp']),('loadorder.txt',['SkyScape.esm','SkyUI_SE.esp'])]:
 path=local/filename;lines=path.read_text(encoding='utf-8-sig').splitlines() if path.exists() else []
 lines=[s for s in lines if s.lstrip('*').lower() not in ('skyscape.esm','skyui_se.esp')]
 lines+=entries;temp=backup/('new-'+filename);temp.write_text('\n'.join(lines)+'\n',encoding='utf-8');save(temp,path)
ini=DOCUMENTS/'SkyrimCustom.ini'
conf=configparser.ConfigParser(strict=False);conf.optionxform=str
if ini.exists():conf.read(ini,encoding='utf-8-sig')
if not conf.has_section('Archive'):conf.add_section('Archive')
conf.set('Archive','bInvalidateOlderFiles','1');conf.set('Archive','sResourceDataDirsFinal','')
if not conf.has_section('Papyrus'):conf.add_section('Papyrus')
conf.set('Papyrus','bEnableLogging','1');conf.set('Papyrus','bEnableTrace','1')
temp=backup/'new-SkyrimCustom.ini'
with temp.open('w') as f:conf.write(f,space_around_delimiters=False)
save(temp,ini)
assert sha(plugin)==before
for name in ('conversion-report.json','asset-audit.json','texture-fixes.tsv'):shutil.copy2((Path(__file__).parent if name == 'texture-fixes.tsv' else root)/name,backup/name)
(root/'last-install.txt').write_text(str(backup))
(backup/'README.txt').write_text('SkyScape 1.2.0 local SE port plus SkyUI 6.11 and required SKSE scripts.\nExisting SkyrimScape DLL preserved: '+before+'\nConversion uses nifly and explicit xEdit schema transformations, not Creation Kit.\nManifest records each installed file and backups of all previous files.\nUse a separate new character. Open System > Mod Configuration > SkyScape > Teleport to enter.\nNo gameplay validation yet.\n')
print('Installed and hash-verified',len(manifest),'files/configurations. Backup:',backup)
print('SkyrimScape DLL preserved:',before)
