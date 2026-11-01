# RTTI и строки DLL игры: vtables_of(класс), cstr(rva), r2o/o2r - перевод RVA <-> смещение в файле.
import sys,re,struct,pickle
sys.path.insert(0,sys.argv[0].rsplit('/',1)[0] if '/' in sys.argv[0] else '.')
from riprefs import d,secs,md
base=0x180000000
# RVA -> смещение в файле DLL (None, если вне секций)
def r2o(r): return next((s[4]+r-s[2] for s in secs if s[2]<=r<s[2]+max(s[1],s[3])),None)
# смещение в файле -> RVA
def o2r(o): return next((s[2]+o-s[4] for s in secs if s[4]<=o<s[4]+s[3]),None)
# все vtable класса Leonardo::<cls> по RTTI: [(rva vtable, смещение подобъекта, [rva функций по слотам])]
def vtables_of(cls):
    off=d.find(('.?AV'+cls+'@Leonardo@@\0').encode())
    td=o2r(off)-0x10; out=[]
    for m in re.finditer(re.escape(struct.pack('<I',td)),d):
        o=m.start()-12
        sig,of,cd,ptd,pch,pself=struct.unpack_from('<IIIIII',d,o)
        if sig==1 and ptd==td and pself==o2r(o):
            for mm in re.finditer(re.escape(struct.pack('<Q',base+o2r(o))),d):
                vt=o2r(mm.start())+8; ents=[]
                k=0
                while True:
                    q=struct.unpack_from('<Q',d,r2o(vt+8*k))[0]-base
                    if not (0x1000<=q<0x6d6000): break
                    ents.append(q); k+=1
                out.append((vt,of,ents))
    return out
# ASCII-строка по RVA (для подписей в листингах) или None
def cstr(t):
    o=r2o(t)
    if o is None: return None
    e=d.find(b'\0',o,o+100); s=d[o:e]
    return s.decode() if e>o+1 and all(32<=c<127 for c in s) else None
