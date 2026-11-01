import sys,os,re,capstone
# python re/mksig.py <rva>:<начало функции>:<нужно совпадений> ...  -  уникальный байтовый шаблон (rip/call замаскированы)
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from cls import d,r2o,o2r
from riprefs import secs
txt=[s for s in secs if s[0]=='.text'][0]   # (name, vsize, va, rsize, raddr)
T0=txt[4]; T=d[T0:T0+txt[3]]; TVA=txt[2]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64); md.detail=True
# дизассемблировать [start, end)
def insns(start,end):
    o=r2o(start); return list(md.disasm(d[o:o+(end-start)],start))
# байты инструкции и маска: rip-смещения и адреса call/jmp - «любой байт» (меняются между сборками)
def masked(ins):
    b=list(ins.bytes); m=[True]*len(b)
    if ins.disp_size and any(op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP for op in ins.operands):
        for i in range(ins.disp_offset,ins.disp_offset+ins.disp_size): m[i]=False
    if ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL):
        if ins.imm_size and ins.operands and ins.operands[0].type==capstone.x86.X86_OP_IMM:
            for i in range(ins.imm_offset,ins.imm_offset+ins.imm_size): m[i]=False
    return b,m
# все адреса в .text, где совпадает шаблон (pb - байты, pm - маска)
def count(pb,pm):
    # regex over .text
    rx=b''.join(re.escape(bytes([x])) if k else b'.' for x,k in zip(pb,pm))
    return [TVA+mm.start() for mm in re.finditer(rx,T,re.S)]
# кратчайший шаблон (>= 20 байт) вокруг target, который встречается ровно expect раз: (шаблон, смещение target, совпадения)
def sig(target,fn_start,expect=1,maxn=24):
    allins=[]
    a=fn_start
    # disassemble function linear from fn_start up to target+0x100
    allins=insns(fn_start,target+0x100)
    idx=[i for i,x in enumerate(allins) if x.address<=target<x.address+x.size][0]
    for n in range(0,maxn):
        for m in range(1,maxn):
            sel=allins[max(0,idx-n):idx+m]
            pb=[];pm=[]
            for x in sel:
                b,mm=masked(x); pb+=b; pm+=mm
            hits=count(pb,pm)
            if len(pb)>=20 and len(hits)==expect and sel[0].address+ (target-sel[0].address) in [h+(target-sel[0].address) for h in hits]:
                s=' '.join('%02X'%x if k else '??' for x,k in zip(pb,pm))
                return s,target-sel[0].address,[hex(h+(target-sel[0].address)) for h in hits]
    return None
if __name__=='__main__':
    for spec in sys.argv[1:]:
        t,f,e=spec.split(':'); print(spec, sig(int(t,16),int(f,16),int(e)))
