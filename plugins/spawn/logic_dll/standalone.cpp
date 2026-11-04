// SpawnFramework.dll on its own: the entry point when our winhttp.dll loads the plugin from bin\plugins. It waits for
// logic.dll and patches it the moment it is loaded (onLogicLoaded, hooks.cpp).

#include "plugin.h"

#ifndef SPAWN_VERSION  // set by CMake (MOD_VERSION)
#define SPAWN_VERSION "dev"
#endif

namespace spawn {

dfd::DllWatcher g_watcher, g_gameWatcher;

}  // namespace spawn

using namespace spawn;

// Loaded by winhttp.dll from its DllMain, under the Windows loader lock: only open the log and wait for logic.dll.
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    g_log.open(self, L"SpawnFramework");
    setGameDir(gameDirOf(self));
    g_log("Spawn Framework " SPAWN_VERSION " loaded, waiting for logic.dll");
    if (!g_watcher.watch(L"logic.dll", &onLogicLoaded)) g_log("cannot register DLL notification");
    if (!g_gameWatcher.watch(L"game.dll", &onGameLoaded)) g_log("cannot register DLL notification for game.dll");
    return TRUE;
}
