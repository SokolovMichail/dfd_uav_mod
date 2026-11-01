# Модуль: DLL игры в памяти (d), её секции (secs), функции по .pdata (funcs), capstone (md), поиск rip-ссылок.
import struct,capstone,sys,re,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from _game import dll_path
d=open(dll_path(),'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
secs=[]
for i in range(nsec):
    o=pe+24+opt+40*i; secs.append((d[o:o+8].rstrip(b'\0').decode(),)+struct.unpack_from('<IIII',d,o+8))
txt=[s for s in secs if s[0]=='.text'][0]; pd=[s for s in secs if s[0]=='.pdata'][0]
funcs=sorted(set(struct.unpack_from('<II',d,pd[4]+12*i) for i in range(pd[1]//12)))
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
# дизассемблировать функцию [b, e) (RVA)
def fn(b,e): return list(md.disasm(d[txt[4]+b-txt[2]:txt[4]+e-txt[2]],b))
# все rip-обращения к адресам из targets {rva: имя}: [(начало, конец функции, rva инструкции, имя)]
def refs(targets):
    out=[]
    for b,e in funcs:
        for i in fn(b,e):
            m=re.search(r'\[rip ([+-]) (0x[0-9a-f]+)\]',i.op_str)
            if m:
                t=i.address+i.size+(int(m.group(2),16)*(1 if m.group(1)=='+' else -1))
                if t in targets: out.append((b,e,i.address,targets[t]))
    return out
