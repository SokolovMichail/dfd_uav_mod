# Crash guard entry, see guard.h. Windows x64 calling convention, GNU as (AT&T syntax).
#
#   int dfd_guard_enter(GuardFrame* frame /*rcx*/, void (*fn)(void*) /*rdx*/, void* arg /*r8*/);
#
# Saves the callee-saved registers, rsp and mxcsr into *frame and calls fn(arg). Returns 0 when fn returns. When
# fn faults, guardHandler puts the saved registers back into the thread's context, sets rax = 1 and rip = the
# landing label, so the call "returns" 1 from here. The function has unwind info (.seh_*), so a C++ exception
# thrown by fn unwinds through it as through any other function.

    .text
    .globl dfd_guard_enter
    .def dfd_guard_enter; .scl 2; .type 32; .endef
    .p2align 4
    .seh_proc dfd_guard_enter
dfd_guard_enter:
    subq $40, %rsp                      # shadow space for fn + alignment to 16
    .seh_stackalloc 40
    .seh_endprologue
    movq %rbx, 0x00(%rcx)
    movq %rbp, 0x08(%rcx)
    movq %rsi, 0x10(%rcx)
    movq %rdi, 0x18(%rcx)
    movq %r12, 0x20(%rcx)
    movq %r13, 0x28(%rcx)
    movq %r14, 0x30(%rcx)
    movq %r15, 0x38(%rcx)
    movq %rsp, 0x40(%rcx)
    leaq dfd_guard_landing(%rip), %rax
    movq %rax, 0x48(%rcx)
    stmxcsr 0x50(%rcx)
    movdqa %xmm6, 0x60(%rcx)
    movdqa %xmm7, 0x70(%rcx)
    movdqa %xmm8, 0x80(%rcx)
    movdqa %xmm9, 0x90(%rcx)
    movdqa %xmm10, 0xa0(%rcx)
    movdqa %xmm11, 0xb0(%rcx)
    movdqa %xmm12, 0xc0(%rcx)
    movdqa %xmm13, 0xd0(%rcx)
    movdqa %xmm14, 0xe0(%rcx)
    movdqa %xmm15, 0xf0(%rcx)
    movq %r8, %rcx
    callq *%rdx
    xorl %eax, %eax
dfd_guard_landing:
    addq $40, %rsp
    ret
    .seh_endproc
