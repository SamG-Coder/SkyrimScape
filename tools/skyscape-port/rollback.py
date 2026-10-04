from pathlib import Path
import json,hashlib,shutil,sys,subprocess
from port_config import ROOT, GAME, LOCAL, DOCUMENTS, BACKUPS, require_game_closed
root=ROOT
backup=Path((root/'last-install.txt').read_text().strip()).resolve()
assert backup.is_relative_to(BACKUPS.resolve())
entries=json.loads((backup/'manifest.json').read_text())
allowed=(GAME/'Data').resolve()
configs={p.resolve() for p in [LOCAL/'plugins.txt',LOCAL/'loadorder.txt',DOCUMENTS/'SkyrimCustom.ini']}
for entry in entries:
 p=Path(entry['destination']).resolve();assert p.is_relative_to(allowed) or p in configs
 if p.exists():assert hashlib.sha256(p.read_bytes()).hexdigest()==entry['sha256'],f'Changed since install; retained: {p}'
 if entry['backup']:assert Path(entry['backup']).resolve().is_relative_to(backup)
print('Verified rollback targets:',len(entries))
if '--apply' not in sys.argv:print('Dry run only. Use --apply after exiting Skyrim and returning out of SkyScape.');sys.exit()
require_game_closed()

for entry in reversed(entries):
 p=Path(entry['destination'])
 if entry['existed']:shutil.copy2(entry['backup'],p)
 elif p.exists():p.unlink()
print('Restored previous files/configuration; SkyrimScape plugin was never changed.')
