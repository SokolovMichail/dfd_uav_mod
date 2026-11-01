// Crash guard for the plugins' own code inside the game (guard.h + guard.cpp + guard_x64.S).
//
// guarded(f) runs f. If f, or game code that f calls, faults (an access violation and the like), the game is not
// brought down: the thread resumes right after the guarded(f) call as if f had returned, and guarded() returns
// true with what happened in the Fault. How:
//   * dfd_guard_enter (guard_x64.S) saves the callee-saved registers and the stack pointer into a GuardFrame on
//     the caller's stack, then calls f;
//   * a vectored exception handler (guardHandler) sees the fault first. If this thread is inside a guarded call
//     and the exception is one of the faults below, it rewrites the faulting thread's context with the saved
//     registers and a landing address in dfd_guard_enter, and lets the thread continue there - no stack unwinding
//     through the game's frames, no longjmp out of the handler;
//   * any other exception (C++ exceptions, breakpoints, stack overflow, a fault outside a guarded call) is passed
//     on untouched.
//
// What a fault skips stays undone: destructors of the frames between the fault and guarded() do not run (memory
// they own leaks) and the game may be left halfway through a change. The caller decides what to put back.
// Guards nest: a fault lands in the innermost one.
#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dfd {

// What a guarded call ran into: the exception code (EXCEPTION_ACCESS_VIOLATION...) and the faulting instruction.
struct Fault {
    DWORD code = 0;
    uintptr_t address = 0;
};

// One guarded call in progress. The register part is filled by dfd_guard_enter, its layout is shared with
// guard_x64.S (checked below); the rest by guardCall / guardHandler.
struct GuardFrame {
    uint64_t rbx, rbp, rsi, rdi, r12, r13, r14, r15;  // +0x00
    uint64_t rsp;                                      // +0x40: inside dfd_guard_enter, after its prologue
    uint64_t landing;                                  // +0x48: where a faulted call resumes
    uint32_t mxcsr;                                    // +0x50
    uint32_t pad;
    alignas(16) uint8_t xmm[10][16];                   // +0x60: xmm6..xmm15
    GuardFrame* prev;                                  // the enclosing guarded call of this thread
    Fault fault;
};
static_assert(offsetof(GuardFrame, rsp) == 0x40 && offsetof(GuardFrame, landing) == 0x48 &&
              offsetof(GuardFrame, mxcsr) == 0x50 && offsetof(GuardFrame, xmm) == 0x60,
              "GuardFrame layout is shared with guard_x64.S");

// Run fn(arg) under the guard. false - it returned normally; true - it faulted, *fault (if given) says how.
bool guardCall(void (*fn)(void*), void* arg, Fault* fault = nullptr);

// guardCall for any callable, usually a lambda capturing by reference.
template <class F>
bool guarded(F&& f, Fault* fault = nullptr) {
    using Fn = std::remove_reference_t<F>;
    return guardCall([](void* p) { (*static_cast<Fn*>(p))(); }, const_cast<void*>(static_cast<const void*>(&f)),
                     fault);
}

// Is the exception one the guard catches: faults of the code itself, not C++ exceptions or debugger events.
bool isGuardedFault(const EXCEPTION_RECORD& r);

// The vectored handler itself (registered by the first guardCall); exposed for the tests.
LONG CALLBACK guardHandler(EXCEPTION_POINTERS* info);

}  // namespace dfd
