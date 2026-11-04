// The game's objects and engine functions. Overview - engine.h.

#include "engine.h"

#include "plugin.h"

namespace spawn::engine {

// ---------------------------------------------------------------- entities

std::string sysnameOf(void* entity) {
    auto s = vcall<const MsvcString*>(entity, 0x48);
    if (!s) return {};
    std::string full(s->c_str(), s->size);
    size_t hash = full.rfind('#');
    return hash == std::string::npos ? full : full.substr(hash + 1);
}

const float* positionOf(void* entity) {
    void* t = field(cacheOf(entity), 0x00);
    return t ? vcall<const float*>(t, 0x58) : nullptr;
}

int teamOf(void* entity) {
    void* t = field(cacheOf(entity), 0x20);
    return t ? vcall<int>(t, 0x50) : -1;
}

std::vector<void*> soldierList(void* squad) {
    if (!vcall<bool>(squad, 0x98, 0x10)) return {};
    void* ai = aiOf(squad);
    void* members = ai ? vcall<void*>(ai, 0x3d0) : nullptr;
    if (!members) return {};
    auto v = vcall<void***>(members, 0x68);
    return std::vector<void*>(v[0], v[1]);
}

RefBlock* entityById(void* world, int id) {
    if (!world) return nullptr;
    void* registry = vcall<void*>(world, 0x10);
    RefBlock* rb = nullptr;
    vcall<RefBlock**>(registry, 0x60, &rb, id);
    if (rb && !rb->object) {
        release(rb);
        return nullptr;
    }
    return rb;
}

const char* teamName(int id) {
    static const char* const kTeams[] = {"player",       "resistance",       "legion",      "cartel",
                                         "abandoned",    "civilian",         "integrators", "marauders",
                                         "founders",     "survivors",        "total_marauders",
                                         "enemy_resistance", "spy",          "player_ally", "total_enemy",
                                         "cartel_enemy"};
    constexpr int kCount = sizeof kTeams / sizeof kTeams[0];
    return id >= 0 && id < kCount ? kTeams[id] : nullptr;
}

// ---------------------------------------------------------------- the ammunition depot

void* depotOf(void* vehicle) {
    void* ai = aiOf(vehicle);
    return ai ? vcall<void*>(ai, 0x1c8) : nullptr;
}

DepotEntry* depotEntry(void* depot, const std::string& name) {
    if (!depot) return nullptr;
    struct EntryVector {
        DepotEntry *begin, *end, *cap;
    };
    auto e = vcall<EntryVector*>(depot, kDepotEntries);
    for (DepotEntry* k = e->begin; k != e->end; ++k)
        if (name == k->name.c_str()) return k;
    return nullptr;
}

// ---------------------------------------------------------------- engine functions

SpawnFn g_spawn;
void* g_spawnCtx;
DetonateFn g_detonate;
PackFn g_pack;
KillerFn g_killer;
KillFn g_kill;

namespace {

using dfd::Image;

constexpr int kSlotSpawnWrapper = 147;
// LogicUtilsImpl slot 147: moves its arguments into place and calls the spawn function with a context (lea at +0x4c,
// call at +0x61).
constexpr const char* kSpawnWrapper =
    "48 83 EC 78 F3 0F 10 84 24 A8 00 00 00 33 C9 C7 44 24 60 FF FF FF FF 49 8B C1 4C 8B 8C 24 A0 00 00 00 4D 8B D0 "
    "48 89 4C 24 58 4C 8B DA 48 89 4C 24 50 4C 8B C0 48 89 4C 24 48 49 8B D2 48 89 4C 24 40 48 89 4C 24 38 48 89 4C "
    "24 30 48 8D 0D ?? ?? ?? ?? 48 89 4C 24 28 49 8B CB F3 0F 11 44 24 20 E8 ?? ?? ?? ?? 48 83 C4 78 C3";
constexpr size_t kSpawnCtxLea = 0x4c, kSpawnCall = 0x61;
constexpr const char* kPrologueSpawn = "48 8B C4 4C 89 48 20 4C 89 40 18 48 89 50 10 48 89 48 08 53 56 57 41 54";
// The self-detonate command's server side (0x1145a0): "AiFacet->vfunc(+0x370)(true); call self-detonate(entity)".
constexpr const char* kSigDetonateCall =
    "48 8B 48 28 48 85 C9 74 24 48 8B 01 B2 01 FF 90 70 03 00 00 48 8B 0B 48 85 C9 74 09 48 8B 49 10 48 85 C9 75 03 "
    "48 8B CE E8 ?? ?? ?? ??";
constexpr size_t kDetonateCall = 0x28;
constexpr const char* kPrologueDetonate = "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 54 41 55 41 56 41 57";
// The mission action that expands / packs a vehicle: "expand" - call deploy(entity, 0), else - call pack(entity).
constexpr const char* kSigPackCall =
    "81 38 65 78 70 61 75 28 66 81 78 04 6E 64 75 20 49 8B 0E 48 85 C9 74 09 48 8B 49 10 48 85 C9 75 03 48 8B CF 33 D2 "
    "E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 49 8B 0E 48 85 C9 74 09 48 8B 49 10 48 85 C9 75 03 48 8B CF E8 ?? ?? ?? ??";
constexpr size_t kPackCall = 0x44;
constexpr const char* kProloguePack = "40 56 57 48 83 EC 38 48 8B 01 48 8B F9 FF 90 B0 00 00 00 48 8B 70 28";
// KillUnitsAction (0x339d0): "world -> kill manager (call at +6), queue a unit (call at +17)"; every match must agree.
constexpr const char* kSigKillCall = "FF 50 78 48 8B C8 E8 ?? ?? ?? ?? 48 8B D6 48 8B C8 E8 ?? ?? ?? ??";
constexpr size_t kKillerCall = 6, kKillCall = 17;
constexpr const char* kPrologueKiller = "40 53 48 83 EC 20 65 48 8B 04 25 58 00 00 00 48 8B D9";
constexpr const char* kPrologueKill = "48 89 5C 24 18 55 56 57 41 56 41 57 48 83 EC 40 0F 29 74 24 30 4C 8B F2";
// AiFacet slots 33 / 34 (set an ability's cooldown timer / length): both look the ability up in the table at
// [AiFacet+0x108] - "mov r10, [rcx+0x108]; mov r9, FNV prime".
constexpr const char* kPrologueAbilitySetter = "4C 8B 91 08 01 00 00 49 B9 B3 01 00 00 00 01 00 00";

uintptr_t rvaOf(const Image& img, const void* p) { return p ? img.rva(reinterpret_cast<uintptr_t>(p)) : 0; }

// The target of the call at `at` if it starts with `prologue`; nullptr - not as expected.
template <class Fn>
Fn callTarget(uintptr_t at, const char* prologue) {
    uintptr_t target = Image::ripTarget(at, 1, 5);
    return Image::matches(target, prologue) ? reinterpret_cast<Fn>(target) : nullptr;
}

}  // namespace

bool findEngine(const Image& img, uintptr_t aiVt, uintptr_t utilsVt) {
    uintptr_t wrapper = *reinterpret_cast<uintptr_t*>(utilsVt + 8 * kSlotSpawnWrapper);
    if (Image::matches(wrapper, kSpawnWrapper)) {
        g_spawnCtx = reinterpret_cast<void*>(Image::ripTarget(wrapper + kSpawnCtxLea, 3, 7));
        g_spawn = callTarget<SpawnFn>(wrapper + kSpawnCall, kPrologueSpawn);
    }
    auto det = img.find(kSigDetonateCall);
    if (det.size() == 1) g_detonate = callTarget<DetonateFn>(det[0] + kDetonateCall, kPrologueDetonate);
    auto pack = img.find(kSigPackCall);
    if (pack.size() == 1) g_pack = callTarget<PackFn>(pack[0] + kPackCall, kProloguePack);

    auto kills = img.find(kSigKillCall);
    for (uintptr_t h : kills) {
        auto killer = callTarget<KillerFn>(h + kKillerCall, kPrologueKiller);
        auto kill = callTarget<KillFn>(h + kKillCall, kPrologueKill);
        if (!killer || !kill || (g_killer && killer != g_killer) || (g_kill && kill != g_kill)) {
            g_killer = nullptr;
            g_kill = nullptr;
            break;
        }
        g_killer = killer;
        g_kill = kill;
    }
    g_log("kill queue: manager %08X queue %08X (%zu calls)%s", rvaOf(img, (void*)g_killer), rvaOf(img, (void*)g_kill),
          kills.size(), g_kill ? "" : " - not found: a squad inside a building or a vehicle outlives its vehicle");

    auto slotOf = [&](size_t off) { return *reinterpret_cast<uintptr_t*>(aiVt + off); };
    bool setters = Image::matches(slotOf(kSetAbilityTimer), kPrologueAbilitySetter) &&
                   Image::matches(slotOf(kSetAbilityLength), kPrologueAbilitySetter);
    g_log("functions: spawn %08X (context %08X), self-detonate %08X (%zu calls), pack %08X (%zu calls), "
          "ability cooldown setters %08X / %08X (%s)",
          rvaOf(img, (void*)g_spawn), rvaOf(img, g_spawnCtx), rvaOf(img, (void*)g_detonate), det.size(),
          rvaOf(img, (void*)g_pack), pack.size(), img.rva(slotOf(kSetAbilityTimer)), img.rva(slotOf(kSetAbilityLength)),
          setters ? "as expected" : "NOT as expected");
    return g_spawn && g_detonate && g_pack && setters;
}

}  // namespace spawn::engine
