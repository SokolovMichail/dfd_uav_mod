# python re/disasm_all.py [out.pkl]  -  дизассемблирует все функции DLL (по .pdata) в кэш re/cache/<dll>.pkl
import struct,capstone,pickle,sys,os
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from _game import dll_path,pkl_path
d=open(dll_path(),'rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]; opt=struct.unpack_from('<H',d,pe+20)[0]
secs=[]
for i in range(nsec):
    o=pe+24+opt+40*i; vs,va,rs,ra=struct.unpack_from('<IIII',d,o+8); secs.append((d[o:o+8].rstrip(b'\0').decode(),va,vs,ra,rs))
pd=[s for s in secs if s[0]=='.pdata'][0]
funcs=sorted(set(struct.unpack_from('<II',d,pd[3]+12*i) for i in range(pd[2]//12)))
txt=[s for s in secs if s[0]=='.text'][0]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64)
out={}
for b,e in funcs:
    code=d[txt[3]+b-txt[1]:txt[3]+e-txt[1]]
    out[b]=[(i.address,i.mnemonic,i.op_str) for i in md.disasm(code,b)]
dst=sys.argv[1] if len(sys.argv)>1 else pkl_path()
os.makedirs(os.path.dirname(dst),exist_ok=True)
pickle.dump(out,open(dst,'wb'))
print('functions',len(out))
