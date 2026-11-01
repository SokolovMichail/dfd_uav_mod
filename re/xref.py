# python re/xref.py <dll> <строка...> - где код ссылается (lea) на строку; рядом - соседние строки.
import struct,re,sys
d=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]
nsec=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
secs=[]
for i in range(nsec):
    o=pe+24+opt+40*i
    name=d[o:o+8].rstrip(b'\0').decode(); vs,va,rs,ra=struct.unpack_from('<IIII',d,o+8)
    secs.append((name,va,vs,ra,rs))
# смещение в файле -> RVA
def off2rva(off):
    for n,va,vs,ra,rs in secs:
        if ra<=off<ra+rs: return va+off-ra
# RVA -> смещение в файле
def rva2off(rva):
    for n,va,vs,ra,rs in secs:
        if va<=rva<va+rs: return ra+rva-va
text=[s for s in secs if s[0]=='.text'][0]
_,tva,tvs,tra,trs=text
code=d[tra:tra+trs]
# collect all rip-relative lea refs: (instr rva, target rva)
refs=[]
for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]',code,re.S):
    i=m.start(); disp=struct.unpack_from('<i',code,i+3)[0]
    refs.append((tva+i, tva+i+7+disp))
# ASCII-строка по RVA или None
def cstr(rva):
    o=rva2off(rva)
    if o is None: return None
    e=d.find(b'\0',o,o+200)
    s=d[o:e]
    if e>o and all(32<=c<127 for c in s): return s.decode()
    return None
import bisect
refs.sort()
keys=sys.argv[2:]
for key in keys:
    for m in re.finditer(b'\0'+key.encode()+b'\0',d):
        srva=off2rva(m.start()+1)
        for ir,tr in refs:
            if tr==srva:
                print(f'=== "{key}" referenced at rva {ir:#x}')
                lo=bisect.bisect_left(refs,(ir-1500,0)); hi=bisect.bisect_left(refs,(ir+1500,0))
                out=[]
                for a,t in refs[lo:hi]:
                    s=cstr(t)
                    if s and len(s)<60: out.append(f'{a-ir:+d}:{s}')
                print('  '+' | '.join(out))
