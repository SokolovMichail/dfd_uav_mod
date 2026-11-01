import pickle,re,sys,os,bisect
# Интерактивный разбор: from h import *  ->  L(rva) листинг, X(rva) ссылки, vt('Класс') vtable.
# Другая DLL: переменная окружения DLL=game.dll. Кэш дизассемблера создаётся сам (re/cache/).
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from _game import pkl_path
import subprocess
if not os.path.isfile(pkl_path()):
    subprocess.run([sys.executable,os.path.join(os.path.dirname(os.path.abspath(__file__)),'disasm_all.py')],check=True)
from cls import vtables_of,cstr,r2o,o2r,d
F=pickle.load(open(pkl_path(),'rb'))
starts=sorted(F)
# начало функции, которой принадлежит адрес a
def owner(a):
    i=bisect.bisect_right(starts,a)-1; return starts[i]
def tgt(a,ins):
    m=re.search(r'\[rip ([+-]) (0x[0-9a-f]+)\]',ins[2])
    if m: return 'rip',None
    return None
# адрес rip-операнда инструкции i (nxt - адрес следующей инструкции) или None
def riptarget(i,nxt):
    m=re.search(r'\[rip ([+-]) (0x[0-9a-f]+)\]',i[2])
    if m: return nxt+(int(m.group(2),16)*(1 if m.group(1)=='+' else -1))
# листинг функции f из кэша (можно ограничить адресами lo..hi), с подписями строк
def pf(f,lo=None,hi=None):
    ins=F[f]
    for k,i in enumerate(ins):
        if lo and i[0]<lo: continue
        if hi and i[0]>hi: break
        nxt=ins[k+1][0] if k+1<len(ins) else None
        t=riptarget(i,nxt) if nxt else None
        extra=''
        if t:
            s=cstr(t); extra=f'   ; ->{t:#x}'+(f' "{s}"' if s else '')
        print(f'{i[0]:#08x}  {i[1]:6} {i[2]}{extra}')
_idx=None
# все места, где адрес t вызывается (call/jmp) или читается rip-операндом: [(функция, адрес, инструкция)]
def xref(t):
    out=[]
    for f,ins in F.items():
        for k,i in enumerate(ins):
            if i[1] in('call','jmp') and i[2]==hex(t): out.append((f,i[0],i[1]))
            elif k+1<len(ins) and riptarget(i,ins[k+1][0])==t: out.append((f,i[0],i[1]))
    return out
# напечатать vtable класса по RTTI: слоты и адреса функций
def vt(cls):
    for v,of,e in vtables_of(cls):
        print(cls,f'vt={v:#x} off={of:#x}')
        for k,x in enumerate(e): print(f'  {k:3} +{8*k:#05x} {x:#x}')
import capstone,struct
_md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
# приблизительный конец функции: до первой пары int3 (cc cc)
def endof(f):
    # follow pdata: function end = start of next function that is not a chained chunk; approximate via int3 padding
    o=r2o(f); e=o
    while d[e:e+2]!=b'\xcc\xcc' : e+=1
    return f+(e-o)
# листинг функции f напрямую из DLL (capstone) до e или до конца функции, с подписями строк
def L(f,e=None,n=4000):
    if e is None: e=endof(f)
    o=r2o(f)
    for i in _md.disasm(d[o:o+(e-f)],f):
        t=None
        m=re.search(r'\[rip ([+-]) (0x[0-9a-f]+)\]',i.op_str)
        extra=''
        if m:
            t=i.address+i.size+(int(m.group(2),16)*(1 if m.group(1)=='+' else -1))
            s=cstr(t); extra=f'   ; ->{t:#x}'+(f' "{s}"' if s else '')
        print(f'{i.address:#08x}  {i.mnemonic:6} {i.op_str}{extra}')
# напечатать ссылки на адрес t (см. xref)
def X(t):
    for f,a,k in xref(t): print(f'  {k} at {a:#x} in fn {f:#x}')
