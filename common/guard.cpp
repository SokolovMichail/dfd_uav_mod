// Crash guard: the vectored handler and guardCall. Overview - guard.h.

#include "guard.h"

extern "C" int dfd_guard_enter(dfd::GuardFrame* frame, void (*fn)(void*), void* arg);

namespace dfd {

namespace {

// The innermost guarded call of this thread; nullptr - the thread is not inside one.
thread_local GuardFrame* t_top = nullptr;

// The handler stays registered while the DLL is loaded and is removed when it unloads (the static's destructor
// runs on DLL_PROCESS_DETACH): a handler left behind in an unloaded DLL would crash the game on its next exception.
struct Registration {
    PVOID handle = AddVectoredExceptionHandler(1, guardHandler);
    ~Registration() {
        if (handle) RemoveVectoredExceptionHandler(handle);
    }
};

// xmm6..xmm15 are copied into the context as one block.
static_assert(offsetof(CONTEXT, Xmm15) - offsetof(CONTEXT, Xmm6) == 9 * 16, "CONTEXT xmm registers in a row");

}  // namespace

bool isGuardedFault(const EXCEPTION_RECORD& r) {
    if (r.ExceptionFlags & EXCEPTION_NONCONTINUABLE) return false;
    switch (r.ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_STACK_CHECK:
        return true;
    default:
        // EXCEPTION_STACK_OVERFLOW too: there is no stack left to land on safely
        return false;
    }
}

// Runs on the faulting thread before any frame-based handler. Only rewrites the context; the frame is popped by
// guardCall once the thread is back there.
LONG CALLBACK guardHandler(EXCEPTION_POINTERS* info) {
    GuardFrame* f = t_top;
    if (!f || !isGuardedFault(*info->ExceptionRecord)) return EXCEPTION_CONTINUE_SEARCH;
    f->fault = {info->ExceptionRecord->ExceptionCode,
                reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress)};
    CONTEXT* c = info->ContextRecord;
    c->Rbx = f->rbx;
    c->Rbp = f->rbp;
    c->Rsi = f->rsi;
    c->Rdi = f->rdi;
    c->R12 = f->r12;
    c->R13 = f->r13;
    c->R14 = f->r14;
    c->R15 = f->r15;
    c->Rsp = f->rsp;
    c->Rip = f->landing;
    c->Rax = 1;  // dfd_guard_enter returns 1: faulted
    c->MxCsr = f->mxcsr;
    c->FltSave.MxCsr = f->mxcsr;
    memcpy(&c->Xmm6, f->xmm, sizeof f->xmm);
    c->EFlags &= ~0x400u;  // direction flag clear, as the calling convention requires
    c->ContextFlags |= CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT;
    return EXCEPTION_CONTINUE_EXECUTION;
}

bool guardCall(void (*fn)(void*), void* arg, Fault* fault) {
    static Registration registration;
    GuardFrame frame{};
    frame.prev = t_top;
    t_top = &frame;
    // popped however the call ends: returned, faulted, or a C++ exception unwinding through it
    struct Pop {
        GuardFrame* prev;
        ~Pop() { t_top = prev; }
    } pop{frame.prev};
    bool faulted = dfd_guard_enter(&frame, fn, arg) != 0;
    if (faulted && fault) *fault = frame.fault;
    return faulted;
}

}  // namespace dfd
