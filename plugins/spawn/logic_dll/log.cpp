// The plugin's log and its crash guard. Declared in plugin.h.

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "plugin.h"

namespace spawn {

Logger g_log;
bool g_logEvents = true;

void Logger::operator()(const char* fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (sink_)
        sink_(line);
    else
        file_("%s", line);
}

// ---------------------------------------------------------------- the crash guard

namespace {

int g_faults = 0;

// "logic.dll+0012ABCD": the module the faulting instruction is in and the offset in it.
void describeAddress(uintptr_t at, char* out, size_t size) {
    HMODULE m = nullptr;
    char path[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(at), &m) &&
        GetModuleFileNameA(m, path, sizeof path)) {
        const char* name = strrchr(path, '\\');
        snprintf(out, size, "%s+%08llX", name ? name + 1 : path,
                 static_cast<unsigned long long>(at - reinterpret_cast<uintptr_t>(m)));
    } else {
        snprintf(out, size, "%p (no module)", reinterpret_cast<void*>(at));
    }
}

}  // namespace

bool g_ownWorkOff = false;

// Always logged: at most kMaxFaults lines per run, and they are what a bug report needs.
void onFault(const char* where, const dfd::Fault& fault) {
    ++g_faults;
    char at[MAX_PATH + 32];
    describeAddress(fault.address, at, sizeof at);
    g_log("fault in %s: exception %08lX at %s - skipped (fault %d of %d)", where, fault.code, at, g_faults, kMaxFaults);
    if (g_faults >= kMaxFaults) {
        g_ownWorkOff = true;
        g_log("too many faults: spawning is off until the game restarts");
    }
}

}  // namespace spawn
