// spawn_framework.dll: the same plugin as a DefianceLoader plugin (https://github.com/DefianceLoader/DefianceLoader).
//
// The loader loads the DLL from DefianceLoader\plugins, with spawn_framework.plugin.json beside it, and calls init()
// once logic.dll is in the process. init finds the same places as the standalone DLL (install) but writes none of
// them itself: every write (three vtable slots) is handed to the loader (patch_bytes), which checks it against the
// other plugins' writes and makes them all together after init. If anything is not found or not as expected, init
// fails and the loader drops everything this plugin asked for. The same list is the patch contract.
//
// The rules and the hooks are the standalone DLL's. Only the entry point, the log and the way the patches go in
// differ. (Same as DFDRepairMod's spares_repair.dll.)

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

// The loader's Patcher. apply() checks every write the way dfd::PatchSet does, then asks the loader for each one
// (patch_bytes) instead of writing it: a vtable slot is 8 bytes, from the original function's address to the hook's.
class DefiancePatcher final : public Patcher {
public:
    explicit DefiancePatcher(const Image& img) : img_(img) {}

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
            g_log("staged: %s (RVA %08X)", it.what, img_.rva(it.at));
            contract_.push_back({"logic.dll", img_.rva(it.at), DEFIANCE_PATCH_KIND_BYTES, it.orig.data(),
                                 it.orig.size(), it.repl.data(), it.repl.size()});
        }
        return true;
    }

    // The writes apply() staged, for the patch contract. Points into this object.
    const std::vector<DefiancePatchContractEntryV1>& contract() const { return contract_; }

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
    std::vector<Item> items_;
    std::vector<DefiancePatchContractEntryV1> contract_;
};

// Kept for the process: the contract points into the patcher.
std::unique_ptr<DefiancePatcher> g_patcher;

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
    g_patcher = std::make_unique<DefiancePatcher>(img);
    if (!install(img, *g_patcher)) {
        g_patcher.reset();  // no contract for a failed init
        return 1;
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
    const auto& entries = g_patcher->contract();
    contract = {1, sizeof contract, entries.data(), entries.size()};
    return &contract;
}
