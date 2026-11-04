// winhttp.dll - the proxy loader.
//
// It lies in the game's bin\ next to game.dll, so it is loaded instead of the system winhttp.dll.
// 1. Forwards every export to the real %SystemRoot%\System32\winhttp.dll (the stubs are in winhttp_exports.S).
// 2. Loads every plugin bin\plugins\*.dll (in alphabetical order). It changes nothing in the game itself.
// Log: bin\plugins\loader.log

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../common/patch.h"

// The real functions' addresses; the stubs in winhttp_exports.S jump through g_orig[i].
static const char* const kExportNames[] = {
#include "winhttp_exports.inc"
};
constexpr size_t kExportCount = sizeof(kExportNames) / sizeof(kExportNames[0]);
extern "C" {
void* g_orig[kExportCount] = {};
}

static dfd::Log g_log;

// A stand-in for a function the system winhttp.dll does not have (another Windows version): if the game calls it,
// it gets 0/FALSE and the error code ERROR_PROC_NOT_FOUND rather than a crash at address zero.
static DWORD_PTR WINAPI missingExport() {
    SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

// The folder module m was loaded from, with a trailing '\' (for us - the game's bin\).
static std::wstring dirOf(HMODULE m) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    while (n && path[n - 1] != L'\\') --n;
    return std::wstring(path, n);
}

// Load the real winhttp.dll from System32 and fill g_orig with the addresses of all its functions named in
// kExportNames - the stubs jump through them. Missing functions are replaced by the stand-in missingExport.
// false - the system DLL could not be loaded (then ours does not load either: DllMain returns FALSE).
static bool loadSystemWinhttp() {
    wchar_t sys[MAX_PATH];
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    std::wstring path = std::wstring(sys, n) + L"\\winhttp.dll";
    HMODULE real = LoadLibraryW(path.c_str());
    if (!real) {
        g_log("cannot load %ls (error %lu)", path.c_str(), GetLastError());
        return false;
    }
    for (size_t i = 0; i < kExportCount; ++i) {
        FARPROC p = GetProcAddress(real, kExportNames[i]);
        if (!p) g_log("system winhttp.dll has no %s", kExportNames[i]);
        g_orig[i] = p ? reinterpret_cast<void*>(p) : reinterpret_cast<void*>(&missingExport);
    }
    return true;
}

// Load every *.dll in dir (bin\plugins\) in alphabetical order, and log what loaded and what did not.
// Subfolders are skipped. One plugin failing does not stop the others.
static void loadPlugins(const std::wstring& dir) {
    std::vector<std::wstring> files;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*.dll").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        g_log("no plugins in %ls", dir.c_str());
        return;
    }
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) files.emplace_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(files.begin(), files.end());

    for (auto& f : files) {
        // LOAD_WITH_ALTERED_SEARCH_PATH: a plugin's dependencies are looked for in bin\plugins\ first.
        HMODULE m = LoadLibraryExW((dir + f).c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (m)
            g_log("loaded %ls", f.c_str());
        else
            g_log("FAILED to load %ls (error %lu)", f.c_str(), GetLastError());
    }
}

// The entry point: when loaded into a process, it connects the system winhttp.dll and, in the game's main process,
// opens the log and loads the plugins. Returns FALSE (the DLL does not load) only if the system winhttp.dll was not
// found - without it the game could not use the network anyway.
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);

    // The built-in browser's helper processes (CEF: "... --type=renderer" and so on) also load winhttp.dll from
    // bin\. In them calls are only forwarded: no plugins and no log
    // (otherwise they would overwrite the main process's logs).
    bool mainProcess = wcsstr(GetCommandLineW(), L"--type=") == nullptr;

    std::wstring plugins = dirOf(self) + L"plugins\\";
    if (mainProcess) {
        CreateDirectoryW(plugins.c_str(), nullptr);
        g_log.open(self, L"plugins\\loader");
        g_log("DFDRepairMod loader started: %ls", GetCommandLineW());
    }

    if (!loadSystemWinhttp()) return FALSE;
    if (!mainProcess) return TRUE;
    // The plugins are loaded right here (under the loader lock), to be in time for logic.dll.
    // So in a plugin's DllMain: no threads to wait for and no LoadLibrary - only registering.
    loadPlugins(plugins);
    return TRUE;
}
