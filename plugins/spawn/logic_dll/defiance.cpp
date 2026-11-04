// spawn_framework.dll: the same plugin as a DefianceLoader plugin (https://github.com/DefianceLoader/DefianceLoader).
//
// The loader loads the DLL from DefianceLoader\plugins, with spawn_framework.plugin.json beside it, and calls init()
// once logic.dll is in the process. init finds the same places as the standalone DLL (install, installPanel) but
// writes none of them itself: every write - three vtable slots in logic.dll, two in game.dll - is handed to the loader
// (patch_bytes), which checks it against the other plugins' writes and makes them all together after init. If
// anything in logic.dll is not found or not as expected, init fails and the loader drops everything this plugin asked
// for; game.dll's part may fail alone (F4 then does not show the cooldown). The same list is the patch contract.
//
// The rules and the hooks are the standalone DLL's. Only the entry point, the log and the way the patches go in
// differ. (Same as DFDRepairMod's spares_repair.dll.)

#include <cstring>
#include <memory>
#include <string>

#include "../../../third_party/defiance/defiance.h"
#include "plugin.h"

#ifndef SPAWN_VERSION  // set by CMake (MOD_VERSION); must match the manifest's "version"
#define SPAWN_VERSION "dev"
#endif

namespace spawn {

namespace {

// Must match "id" in spawn_framework.plugin.json: the loader checks it, and settings are looked up by it.
constexpr const char kPluginId[] = "sokolov.spawn-framework";

const DefianceApi* g_api = nullptr;
HMODULE g_self = nullptr;

// Log lines go to DefianceLoader\logs\defiance-loader.log, marked with the plugin's ID by the loader.
void toLoader(const char* line) { g_api->log(DEFIANCE_LOG_INFO, line); }

// A bool setting from the manifest, as the loader validated it ("true" / "false"). fallback - no such value.
bool setting(const char* key, bool fallback) {
    const char* v = g_api->config_get(kPluginId, key);
    return v ? strcmp(v, "true") == 0 : fallback;
}

// The loaded module the code at addr belongs to; nullptr - none.
HMODULE moduleAt(const void* addr) {
    HMODULE m = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCWSTR>(addr), &m);
    return m;
}
// A module's file name without the folder, e.g. "SpawnFramework.dll".
std::string moduleName(HMODULE m) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
    const wchar_t* name = path + n;
    while (name > path && name[-1] != L'\\') --name;
    char utf8[MAX_PATH * 3];
    int len = WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8, sizeof utf8, nullptr, nullptr);
    return len > 0 ? utf8 : "?";
}

// The writes every patcher staged, for the patch contract (entries point into the patchers, kept for the process).
std::vector<DefiancePatchContractEntryV1> g_contract;

// The loader's Patcher. apply() checks every write the way dfd::PatchSet does, then asks the loader for each one
// (patch_bytes) instead of writing it: a vtable slot is 8 bytes, from the original function's address to the hook's.
class DefiancePatcher final : public Patcher {
public:
    DefiancePatcher(const Image& img, const char* module) : img_(img), module_(module) {}

    void slot(const char* what, uintptr_t vtable, int slot, const char* prologue, void* hook,
              void** saved) override {
        uintptr_t at = vtable ? vtable + 8u * slot : 0;
        uintptr_t cur = at ? *reinterpret_cast<uintptr_t*>(at) : 0, repl = reinterpret_cast<uintptr_t>(hook);
        Item it{what, at, std::vector<uint8_t>(8), std::vector<uint8_t>(8), saved, reinterpret_cast<void*>(cur),
                prologue};
        memcpy(it.orig.data(), &cur, 8);
        memcpy(it.repl.data(), &repl, 8);
        items_.push_back(std::move(it));
    }

    bool apply() override {
        for (auto& it : items_) {
            if (!it.at) {
                g_log("not found: %s - nothing patched", it.what);
                return false;
            }
            if (!Image::matches(reinterpret_cast<uintptr_t>(it.savedValue), it.prologue)) {
                // A slot that points outside logic.dll was redirected by another mod - most likely the standalone
                // SpawnFramework.dll, which patches logic.dll the moment it loads, before any plugin.
                HMODULE owner = moduleAt(it.savedValue);
                if (owner && reinterpret_cast<uintptr_t>(owner) != img_.base())
                    g_log("verify FAILED: %s at RVA %08X already points into %s - another mod patched it first "
                          "(both Spawn Framework builds installed? remove bin\\plugins\\SpawnFramework.dll) - "
                          "nothing patched",
                          it.what, img_.rva(it.at), moduleName(owner).c_str());
                else
                    g_log("verify FAILED: %s at RVA %08X - nothing patched", it.what, img_.rva(it.at));
                return false;
            }
        }
        for (auto& it : items_) {
            if (g_api->patch_bytes(reinterpret_cast<void*>(it.at), it.orig.data(), it.repl.data(), it.orig.size()) !=
                0) {
                g_log("refused by the loader: %s at RVA %08X - nothing patched", it.what, img_.rva(it.at));
                return false;
            }
            *it.saved = it.savedValue;
            g_log("staged: %s (%s RVA %08X)", it.what, module_, img_.rva(it.at));
            g_contract.push_back({module_, img_.rva(it.at), DEFIANCE_PATCH_KIND_BYTES, it.orig.data(), it.orig.size(),
                                  it.repl.data(), it.repl.size()});
        }
        return true;
    }

private:
    struct Item {
        const char* what;
        uintptr_t at;
        std::vector<uint8_t> orig, repl;
        void** saved;
        void* savedValue;
        const char* prologue;
    };
    Image img_;
    const char* module_;
    std::vector<Item> items_;
};

// Kept for the process: the contract points into the patchers.
std::unique_ptr<DefiancePatcher> g_patcher, g_panelPatcher;

// Called by the loader on its startup thread, logic.dll already loaded. 0 - every write is staged.
int32_t init(const DefianceApi* api) {
    if (!api || api->abi_version != DEFIANCE_ABI_VERSION || api->reserved != 0) return 1;
    g_api = api;
    g_log.sendTo(&toLoader);
    g_logEvents = setting("log", true);
    setGameDir(gameDirOf(g_self));
    g_log("Spawn Framework " SPAWN_VERSION " as a DefianceLoader plugin; event log %s", g_logEvents ? "on" : "off");
    void* base = api->module_base("logic.dll");
    if (!base) {
        g_log("logic.dll is not loaded - nothing patched");
        return 1;
    }
    Image img(reinterpret_cast<uintptr_t>(base));
    g_patcher = std::make_unique<DefiancePatcher>(img, "logic.dll");
    if (!install(img, *g_patcher)) {
        g_patcher.reset();  // no contract for a failed init
        g_contract.clear();
        return 1;
    }
    // The F4 cooldown in game.dll: without it spawning works, so a failure here does not fail init.
    if (void* game = api->module_base("game.dll")) {
        Image gameImg(reinterpret_cast<uintptr_t>(game));
        g_panelPatcher = std::make_unique<DefiancePatcher>(gameImg, "game.dll");
        size_t before = g_contract.size();
        if (!installPanel(gameImg, *g_panelPatcher)) g_contract.resize(before);
    }
    g_log("Spawn Framework staged: active once DefianceLoader commits its patch plan");
    return 0;
}

// The loader restores every write it made for this plugin; nothing of ours to undo.
void stop() { g_log("Spawn Framework stopped"); }

const DefiancePlugin kPlugin = {DEFIANCE_ABI_VERSION, kPluginId, SPAWN_VERSION, &init, &stop};

}  // namespace

}  // namespace spawn

using namespace spawn;

// Only remembers the module, for the game folder (the loader calls init later, not under the loader lock).
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(self);
        g_self = self;
    }
    return TRUE;
}

// The export the loader looks for. The struct lives for the process.
extern "C" __declspec(dllexport) const DefiancePlugin* defiance_plugin() { return &kPlugin; }

// The writes init staged, for the loader to compare with what it got. null - init did not succeed.
extern "C" __declspec(dllexport) const DefiancePatchContractV1* defiance_patch_contract_v1(const DefianceApi*) {
    static DefiancePatchContractV1 contract{};
    if (!g_patcher) return nullptr;
    contract = {1, sizeof contract, g_contract.data(), g_contract.size()};
    return &contract;
}
