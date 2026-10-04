from pathlib import Path
import struct,zlib,json,collections
from port_config import ROOT
def records(data,start=0,end=None):
 end=len(data) if end is None else end
 while start<end:
  typ=data[start:start+4].decode('ascii'); size=struct.unpack_from('<I',data,start+4)[0]
  if typ=='GRUP':
   yield from records(data,start+24,start+size); start+=size;continue
  flags,fid=struct.unpack_from('<II',data,start+8)
  ver=struct.unpack_from('<H',data,start+20)[0]
  body=data[start+24:start+24+size]
  if flags&0x40000:
   expected=struct.unpack_from('<I',body)[0];body=zlib.decompress(body[4:]);assert len(body)==expected
  yield typ,fid,ver,body
  start+=24+size
 assert start==end
def subs(body):
 p=0;longsize=None
 while p<len(body):
  tag=body[p:p+4].decode('ascii');size=struct.unpack_from('<H',body,p+4)[0];p+=6
  if tag=='XXXX':longsize=struct.unpack_from('<I',body,p)[0];p+=size;continue
  if longsize is not None:size=longsize;longsize=None
  yield tag,body[p:p+size];p+=size
 assert p==len(body)
def bsa(path,dest):
 data=path.read_bytes();magic,version,off,flags,nfolders,nfiles,foldernames,filenames,fileflags=struct.unpack_from('<4s8I',data)
 assert magic==b'BSA\x00' and version in (104,105)
 p=off; folders=[]
 for _ in range(nfolders):
  if version==104:h,count,offset=struct.unpack_from('<QII',data,p);p+=16
  else:h,count,unused,offset=struct.unpack_from('<QIIQ',data,p);p+=24
  folders.append(count)
 entries=[]
 for count in folders:
  length=data[p];p+=1;folder=data[p:p+length].rstrip(b'\0').decode();p+=length
  for _ in range(count):
   h,size,offset=struct.unpack_from('<QII',data,p);p+=16;entries.append((folder,size,offset))
 names=data[p:p+filenames].split(b'\0');assert len(entries)==nfiles
 for (folder,size,offset),name in zip(entries,names):
  rel=Path((folder+'\\'+name.decode()).replace('\\','/'));target=(dest/rel).resolve()
  assert target.is_relative_to(dest.resolve())
  raw=data[offset:offset+(size&0x3fffffff)]
  if flags&0x100:raw=raw[1+raw[0]:]
  if bool(flags&4)^bool(size&0x40000000):
   expected=struct.unpack_from('<I',raw)[0]
   if version==104:raw=zlib.decompress(raw[4:])
   else:
    import lz4.block
    raw=lz4.block.decompress(raw[4:],uncompressed_size=expected)
   assert len(raw)==expected
  target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
 return version,nfiles
if __name__=='__main__':
 for fn in [ROOT/'original/SkyScape.esm',ROOT/'skyui/SkyUI_SE.esp']:
  recs=list(records(fn.read_bytes())); print(fn.name,'records',len(recs),'versions',dict(collections.Counter(r[2] for r in recs)))
  print('masters',[v.rstrip(b'\0').decode() for t,v in subs(recs[0][3]) if t=='MAST'])
  print('types',dict(collections.Counter(r[0] for r in recs)))
  for r in recs:
   list(subs(r[3]))
 print('SkyUI BSA',bsa(ROOT/'skyui/SkyUI_SE.bsa',ROOT/'skyui-inspect'))
 print('SkyUI files',dict(collections.Counter(p.suffix for p in (ROOT/'skyui-inspect').rglob('*') if p.is_file())))
