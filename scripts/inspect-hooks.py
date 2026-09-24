from pathlib import Path
import csv,struct,pefile,capstone
exe=Path(r'D:\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe')
pe=pefile.PE(str(exe)); base=pe.OPTIONAL_HEADER.ImageBase
rows={int(r['aeid']):int(r['ae_addr'],16) for r in csv.DictReader(open('external/address-library/offsets-1-7-104.0.csv'))}
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
for name,id,slots in [('PlayerControls',208694,[1]),('ThirdPersonState',205236,[4,5]),('HUDMenu',215362,[5])]:
 for slot in slots:
  addr=struct.unpack('<Q',pe.get_data(rows[id]+slot*8,8))[0]
  print(name,slot,hex(addr-base))
  for i in md.disasm(pe.get_data(addr-base,100),addr):
   print(hex(i.address-base),i.mnemonic,i.op_str)
   if i.mnemonic=='ret':break
