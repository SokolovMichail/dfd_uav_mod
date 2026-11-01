// SpawnProbe.dll - a throwaway probe for the Spawn Framework (Phase 0). Not a mod: it checks in the real game what
// the framework is going to rely on, and writes what it sees to bin\plugins\SpawnProbe.log.
//
// Only vehicles whose sysname starts with `filter` (Probe_) are probed. When one goes into F4 mode (the AI state
// AiDoRechargeState runs for the first time) the probe:
//   1. logs the vehicle: sysname, position, team id and the team's name;
//   2. lists the vehicle's ammunition depot (AmmunitionDepotHelper) and spends one round of `ammo` (the UAV kit) the
//      way firing does; the vehicle has the kit but none left - no spawn;
//   3. spawns its squad (`squad_<vehicle sysname>`, else `squad`) of the vehicle's own team a few metres away, through
//      the engine's own spawn function (the one behind Lua game.spawnSquad), and logs the new squad, id and team id.
// When the state ends (packing up, the vehicle destroyed, another order) it kills that squad: `kill=detonate` - the
// engine's own self-detonation of bomb drones (every soldier takes a lethal hit and dies as if hit: falls, explodes
// with its explode_damage); `kill=queue` - every soldier into the engine's kill queue, as the mission action
// killAction does (they die but do not fall).
//
// Settings - bin\plugins\SpawnProbe.ini, [probe]: filter (Probe_), squad (Lgn_swarm), squad_<vehicle sysname>,
// ammo (uav_kit), offset (8 m), max_spawns (10 per game run), kill (detonate). Without the file the defaults are used.
//
// What is in logic.dll (GOG 1.09.1221, see docs/Probe.md): AiDoRechargeState vtable - slot 0 destructor (0xbfc50,
// switches the station off), slot 6 per-tick (0xbffc0, switches the station on if it is off); LogicUtilsImpl slot 147
// (0x2fde50) - the spawn wrapper, which calls the spawn function (0x526d10) with a context pointer (lea at +0x4c);
// KillUnitsAction (0x339d0) - "world -> kill manager (0x542360) -> queue a unit (0x292d70)"; the server side of the
// self-detonate command (0x1145a0) - "AiFacet +0x370(true), then self-detonate (0x532f60)".

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../common/patch.h"

namespace {

using dfd::Image;

dfd::Log g_log;
dfd::DllWatcher g_watcher;

// ---------------------------------------------------------------- settings

struct Settings {
    std::string filter = "Probe_", squad = "Lgn_swarm", ammo = "uav_kit", kill = "detonate";
    float offset = 8.0f;
    int maxSpawns = 10;
} g_set;

std::wstring g_ini;  // bin\plugins\SpawnProbe.ini

std::string iniString(const std::string& key, const std::string& fallback) {
    std::wstring wkey(key.begin(), key.end());
    wchar_t wbuf[256];
    DWORD n = GetPrivateProfileStringW(L"probe", wkey.c_str(), L"", wbuf, 256, g_ini.c_str());
    if (n == 0) return fallback;
    char buf[256];
    int len = WideCharToMultiByte(CP_UTF8, 0, wbuf, static_cast<int>(n), buf, sizeof buf - 1, nullptr, nullptr);
    return std::string(buf, len > 0 ? len : 0);
}

void readSettings(HMODULE self) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    while (n && path[n - 1] != L'\\') --n;
    g_ini = std::wstring(path, n) + L"SpawnProbe.ini";
    g_set.filter = iniString("filter", g_set.filter);
    g_set.squad = iniString("squad", g_set.squad);
    g_set.ammo = iniString("ammo", g_set.ammo);
    g_set.offset = strtof(iniString("offset", "8").c_str(), nullptr);
    g_set.maxSpawns = atoi(iniString("max_spawns", "10").c_str());
    g_set.kill = iniString("kill", g_set.kill);
}

// ---------------------------------------------------------------- engine objects

// Engine reference block: [+0x08] strong count, [+0x10] the object.
struct RefBlock {
    void** vtbl;
    int32_t strong, weak;
    void* object;
};

template <class R, class... A>
R vcall(void* obj, size_t offset, A... args) {
    auto vtbl = *reinterpret_cast<void***>(obj);
    return reinterpret_cast<R (*)(void*, A...)>(vtbl[offset / 8])(obj, args...);
}

void* objectOf(void* refBlock) { return refBlock ? static_cast<RefBlock*>(refBlock)->object : nullptr; }
void* field(void* obj, size_t off) { return *reinterpret_cast<void**>(static_cast<char*>(obj) + off); }
// Give back a strong reference taken from the engine, the way the engine does it (free at zero).
void release(RefBlock* rb) {
    if (rb && --rb->strong == 0) vcall<void>(rb, 0x10);
}

// std::string as the game's MSVC runtime lays it out. Built here, only read by the game.
struct MsvcString {
    union {
        char buf[16];
        const char* ptr;
    };
    size_t size, cap;
    const char* c_str() const { return cap > 15 ? ptr : buf; }
};
// An MsvcString over a string the probe keeps alive (`keep` must outlive it).
MsvcString msvc(const std::string& keep) {
    MsvcString s{};
    s.size = keep.size();
    if (keep.size() <= 15) {
        memcpy(s.buf, keep.c_str(), keep.size() + 1);
        s.cap = 15;
    } else {
        s.ptr = keep.c_str();
        s.cap = keep.size();
    }
    return s;
}

// The engine's spawn function: (position, world, team name, squad sysname, angle, context, 6 x null, -1).
using SpawnFn = void* (*)(const float* pos, void* world, const MsvcString* team, const MsvcString* sysname,
                          float angle, void* ctx, void*, void*, void*, void*, void*, void*, int);
using KillerFn = void* (*)(void* world);          // the world's kill manager
using KillFn = void (*)(void* killer, void* unit);  // queue a unit to be killed
using DetonateFn = void (*)(void* entity);          // self-detonate an entity (a squad: all its soldiers)
using DtorFn = void* (*)(void* self, unsigned flags);
using TickFn = void (*)(void* self);

SpawnFn g_spawn;
void* g_spawnCtx;
KillerFn g_killer;
KillFn g_kill;
DetonateFn g_detonate;
DtorFn g_origDtor;
TickFn g_origTick;

uintptr_t g_logicBase;
std::unordered_set<void*> g_seen;          // AiDoRechargeState objects already looked at
std::unordered_map<void*, int> g_squadOf;  // probed vehicle -> id of the squad it spawned
int g_spawns = 0;

// Team names by id: the rows of forces.xml in order (run 2: the spawned squad's id matched).
const char* const kTeams[] = {"player",       "resistance", "legion",          "cartel",      "abandoned", "civilian",
                              "integrators",  "marauders",  "founders",        "survivors",   "total_marauders",
                              "enemy_resistance", "spy",    "player_ally",     "total_enemy", "cartel_enemy"};
constexpr int kTeamCount = sizeof kTeams / sizeof kTeams[0];

// The supply component an AiDoRechargeState runs ([state+0x20] reference block) and the vehicle it belongs to.
void* stationOf(void* state) { return objectOf(field(state, 0x20)); }
void* vehicleOf(void* station) { return station ? objectOf(field(station, 0x10)) : nullptr; }

// The entity's prototype, "scripts/species/cars.txt#Res_van" (vfunc +0x48), and its sysname - the part after '#'.
std::string sysnameOf(void* entity) {
    auto s = vcall<const MsvcString*>(entity, 0x48);
    if (!s) return {};
    std::string full(s->c_str(), s->size);
    size_t hash = full.rfind('#');
    return hash == std::string::npos ? full : full.substr(hash + 1);
}

// entity->vfunc(+0xb0): the entity's facet cache. +0x00 transform (vfunc +0x58 -> float[3]), +0x20 TeamFacet
// (vfunc +0x50 -> team id), +0x28 AiFacet (vfunc +0x1c8 -> AmmunitionDepotHelper, +0x3d0 -> a squad's soldiers).
void* cacheOf(void* entity) { return vcall<void*>(entity, 0xb0); }
const float* positionOf(void* entity) {
    void* t = field(cacheOf(entity), 0x00);
    return t ? vcall<const float*>(t, 0x58) : nullptr;
}
int teamOf(void* entity) {
    void* t = field(cacheOf(entity), 0x20);
    return t ? vcall<int>(t, 0x50) : -1;
}
void* aiOf(void* entity) { return field(cacheOf(entity), 0x28); }
void* depotOf(void* entity) {
    void* ai = aiOf(entity);
    return ai ? vcall<void*>(ai, 0x1c8) : nullptr;
}
void* worldOf(void* entity) { return vcall<void*>(entity, 0x78); }

// The live entity with this id, with a strong reference the caller releases; nullptr - gone.
// world->vfunc(+0x10) is the entity registry, its vfunc(+0x60)(&ref, id) the lookup (as Lua game.disappear does).
RefBlock* entityById(void* world, int id) {
    void* registry = vcall<void*>(world, 0x10);
    RefBlock* rb = nullptr;
    vcall<RefBlock**>(registry, 0x60, &rb, id);
    if (rb && !rb->object) {
        release(rb);
        return nullptr;
    }
    return rb;
}

// One depot entry, 0x48 bytes: +0 ammunition row, +8 its sysname, +0x28 capacity, +0x2c rounds (what the ammo
// panel shows), +0x30 (? 1 in run 2), +0x34 a divisor.
struct DepotEntry {
    void* ammo;
    MsvcString name;
    int32_t capacity, rounds, f30, f34;
    uint8_t rest[0x48 - 0x38];
};
static_assert(sizeof(DepotEntry) == 0x48, "depot entry");
struct EntryVector {
    DepotEntry *begin, *end, *cap;
};

// Kit check: log the depot and spend one round of g_set.ammo (depot slot 22 +0xb0 consume(ammo, n) - takes it from
// the rounds and calls the depot's change notification). false - the vehicle carries the kit but has none left.
bool spendKit(void* vehicle) {
    void* depot = depotOf(vehicle);
    if (!depot) {
        g_log("  depot: none - spawning without a kit");
        return true;
    }
    auto v = vcall<EntryVector*>(depot, 0x48);
    g_log("  depot %p: %zu entries", depot, static_cast<size_t>(v->end - v->begin));
    DepotEntry* kit = nullptr;
    for (DepotEntry* e = v->begin; e != v->end; ++e) {
        g_log("    %-20s capacity=%d rounds=%d f30=%d f34=%d", e->name.c_str(), e->capacity, e->rounds, e->f30, e->f34);
        if (g_set.ammo == e->name.c_str()) kit = e;
    }
    if (!kit) {
        g_log("  depot: no '%s' - spawning without a kit", g_set.ammo.c_str());
        return true;
    }
    if (kit->rounds < 1) {
        g_log("  depot: no '%s' left - no spawn", g_set.ammo.c_str());
        return false;
    }
    int before = kit->rounds;
    vcall<void>(depot, 0xb0, kit->ammo, 1u);
    g_log("  depot: spent 1 '%s' (rounds %d -> %d, f30 %d)", g_set.ammo.c_str(), before, kit->rounds, kit->f30);
    return true;
}

void probeSpawn(void* vehicle, const std::string& vehicleName, int team) {
    if (g_spawns >= g_set.maxSpawns) {
        g_log("  spawn: max_spawns (%d) reached - skipped", g_set.maxSpawns);
        return;
    }
    if (team < 0 || team >= kTeamCount) {
        g_log("  spawn: team id %d has no name - skipped", team);
        return;
    }
    const float* p = positionOf(vehicle);
    if (!p) {
        g_log("  spawn: no position - skipped");
        return;
    }
    if (!spendKit(vehicle)) return;
    float pos[3] = {p[0] + g_set.offset, p[1], p[2]};
    std::string teamName = kTeams[team], squadName = iniString("squad_" + vehicleName, g_set.squad);
    MsvcString teamStr = msvc(teamName), squad = msvc(squadName);
    g_log("  spawn: '%s' team '%s' at (%.1f, %.1f, %.1f)", squadName.c_str(), teamName.c_str(), pos[0], pos[1],
          pos[2]);
    ++g_spawns;
    void* e = g_spawn(pos, worldOf(vehicle), &teamStr, &squad, 0.0f, g_spawnCtx, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, -1);
    if (!e) {
        g_log("  spawn: returned null");
        return;
    }
    int id = vcall<int>(e, 0x40), got = teamOf(e);
    g_squadOf[vehicle] = id;
    g_log("  spawn: squad %p id %d '%s', team id %d (%s)", e, id, sysnameOf(e).c_str(), got,
          got == team ? "same as the vehicle" : "DIFFERENT from the vehicle");
}

// Kill the squad with this id. detonate - as the self-detonate command does: the squad's AiFacet +0x370(true), then
// the self-detonation. queue - as KillUnitsAction does: a squad's soldiers (vfunc +0x98(0x10) - "is a squad",
// AiFacet +0x3d0 -> +0x68 the soldiers) one by one into the kill queue; anything else - itself.
void killSquad(void* world, int id) {
    RefBlock* rb = entityById(world, id);
    if (!rb) {
        g_log("  kill: squad %d is already gone", id);
        return;
    }
    void* squad = rb->object;
    if (g_set.kill == "detonate") {
        if (void* ai = aiOf(squad)) vcall<void>(ai, 0x370, true);
        g_detonate(squad);
        g_log("  kill: squad %d '%s' self-detonated", id, sysnameOf(squad).c_str());
    } else {
        std::vector<void*> units;
        void* ai = vcall<bool>(squad, 0x98, 0x10) ? aiOf(squad) : nullptr;
        void* members = ai ? vcall<void*>(ai, 0x3d0) : nullptr;
        if (members) {
            auto v = vcall<void***>(members, 0x68);
            for (void** m = v[0]; m != v[1]; ++m) units.push_back(*m);
        } else {
            units.push_back(squad);
        }
        void* killer = g_killer(world);
        for (void* u : units) g_kill(killer, u);
        g_log("  kill: squad %d '%s' - %zu unit(s) queued (%s)", id, sysnameOf(squad).c_str(), units.size(),
              members ? "soldiers" : "the entity itself");
    }
    release(rb);
}

// AiDoRechargeState slot 6 - runs every tick while the vehicle is in F4 mode.
void hookTick(void* state) {
    g_origTick(state);
    if (!g_seen.insert(state).second) return;
    void* station = stationOf(state);
    void* vehicle = vehicleOf(station);
    if (!vehicle) return;
    std::string name = sysnameOf(vehicle);
    if (name.compare(0, g_set.filter.size(), g_set.filter) != 0) return;  // not ours
    int team = teamOf(vehicle);
    const float* p = positionOf(vehicle);
    g_log("F4 start: '%s' %p (station vtable RVA %08X) team id %d (%s) at (%.1f, %.1f, %.1f)", name.c_str(), vehicle,
          static_cast<unsigned>(*static_cast<uintptr_t*>(station) - g_logicBase), team,
          team >= 0 && team < kTeamCount ? kTeams[team] : "?", p ? p[0] : 0, p ? p[1] : 0, p ? p[2] : 0);
    auto prev = g_squadOf.find(vehicle);
    if (prev != g_squadOf.end()) {
        RefBlock* rb = entityById(worldOf(vehicle), prev->second);
        g_log("  previous squad %d: %s", prev->second, rb ? "still alive" : "gone");
        release(rb);
    }
    probeSpawn(vehicle, name, team);
}

// AiDoRechargeState slot 0 - the deleting destructor: the F4 state ends, for whatever reason.
void* hookDtor(void* state, unsigned flags) {
    if (g_seen.erase(state)) {
        void* vehicle = vehicleOf(stationOf(state));
        auto it = vehicle ? g_squadOf.find(vehicle) : g_squadOf.end();
        if (it != g_squadOf.end()) {
            g_log("F4 end: '%s' %p (flags %u)", sysnameOf(vehicle).c_str(), vehicle, flags);
            killSquad(worldOf(vehicle), it->second);
            g_squadOf.erase(it);
        }
    }
    return g_origDtor(state, flags);
}

// ---------------------------------------------------------------- finding and patching

constexpr int kSlotDtor = 0, kSlotTick = 6, kSlotSpawnWrapper = 147;
constexpr const char* kPrologueDtor = "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 48 8D 05";
constexpr const char* kPrologueTick = "40 53 48 83 EC 20 48 8B D9 48 8B 49 20 48 85 C9";
// LogicUtilsImpl slot 147: moves its arguments into place and calls the spawn function with a context (lea at +0x4c,
// call at +0x61).
constexpr const char* kSpawnWrapper =
    "48 83 EC 78 F3 0F 10 84 24 A8 00 00 00 33 C9 C7 44 24 60 FF FF FF FF 49 8B C1 4C 8B 8C 24 A0 00 00 00 4D 8B D0 "
    "48 89 4C 24 58 4C 8B DA 48 89 4C 24 50 4C 8B C0 48 89 4C 24 48 49 8B D2 48 89 4C 24 40 48 89 4C 24 38 48 89 4C "
    "24 30 48 8D 0D ?? ?? ?? ?? 48 89 4C 24 28 49 8B CB F3 0F 11 44 24 20 E8 ?? ?? ?? ?? 48 83 C4 78 C3";
constexpr size_t kSpawnCtxLea = 0x4c, kSpawnCall = 0x61;
constexpr const char* kPrologueSpawn = "48 8B C4 4C 89 48 20 4C 89 40 18 48 89 50 10 48 89 48 08 53 56 57 41 54";
// KillUnitsAction: "owner->vfunc(+0x78) (world); call kill manager; rdx = unit; call queue" - twice in it.
constexpr const char* kSigKillCall = "FF 50 78 48 8B C8 E8 ?? ?? ?? ?? 48 8B D6 48 8B C8 E8 ?? ?? ?? ??";
constexpr size_t kKillerCall = 6, kKillCall = 17;
constexpr const char* kPrologueKiller = "40 53 48 83 EC 20 65 48 8B 04 25 58 00 00 00 48 8B D9";
constexpr const char* kPrologueKill = "48 89 5C 24 18 55 56 57 41 56 41 57 48 83 EC 40 0F 29 74 24 30 4C 8B F2";
// The self-detonate command's server side: "AiFacet->vfunc(+0x370)(true); call self-detonate(entity)".
constexpr const char* kSigDetonateCall =
    "48 8B 48 28 48 85 C9 74 24 48 8B 01 B2 01 FF 90 70 03 00 00 48 8B 0B 48 85 C9 74 09 48 8B 49 10 48 85 C9 75 03 "
    "48 8B CE E8 ?? ?? ?? ??";
constexpr size_t kDetonateCall = 0x28;
constexpr const char* kPrologueDetonate = "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 54 41 55 41 56 41 57";

// Find the kill manager and the kill queue from the calls in KillUnitsAction (every match must agree), and the
// self-detonation from the self-detonate command.
bool resolveKill(const Image& img) {
    auto hits = img.find(kSigKillCall);
    for (uintptr_t h : hits) {
        auto killer = reinterpret_cast<KillerFn>(Image::ripTarget(h + kKillerCall, 1, 5));
        auto kill = reinterpret_cast<KillFn>(Image::ripTarget(h + kKillCall, 1, 5));
        if ((g_killer && killer != g_killer) || (g_kill && kill != g_kill)) {
            g_log("kill calls disagree");
            return false;
        }
        g_killer = killer;
        g_kill = kill;
    }
    if (!g_killer || !Image::matches(reinterpret_cast<uintptr_t>(g_killer), kPrologueKiller) ||
        !Image::matches(reinterpret_cast<uintptr_t>(g_kill), kPrologueKill)) {
        g_log("kill functions not found (%zu calls)", hits.size());
        return false;
    }
    auto det = img.find(kSigDetonateCall);
    if (det.size() == 1) g_detonate = reinterpret_cast<DetonateFn>(Image::ripTarget(det[0] + kDetonateCall, 1, 5));
    if (!g_detonate || !Image::matches(reinterpret_cast<uintptr_t>(g_detonate), kPrologueDetonate)) {
        g_log("self-detonation not found (%zu calls)", det.size());
        return false;
    }
    g_log("kill: manager %08X queue %08X (%zu calls), self-detonate %08X", img.rva(reinterpret_cast<uintptr_t>(g_killer)),
          img.rva(reinterpret_cast<uintptr_t>(g_kill)), hits.size(), img.rva(reinterpret_cast<uintptr_t>(g_detonate)));
    return true;
}

void onLogicLoaded(uintptr_t base) {
    Image img(base);
    g_logicBase = base;
    g_log("logic.dll at %p (TimeDateStamp %08X)", reinterpret_cast<void*>(base), img.timeDateStamp());
    uintptr_t stateVt = img.vtableOf("AiDoRechargeState@Leonardo");
    uintptr_t utilsVt = img.vtableOf("LogicUtilsImpl@Leonardo");
    g_log("vtables: AiDoRechargeState %08X LogicUtilsImpl %08X", stateVt ? img.rva(stateVt) : 0,
          utilsVt ? img.rva(utilsVt) : 0);
    if (!stateVt || !utilsVt) {
        g_log("vtable not found - nothing patched");
        return;
    }
    uintptr_t wrapper = *reinterpret_cast<uintptr_t*>(utilsVt + 8 * kSlotSpawnWrapper);
    if (!Image::matches(wrapper, kSpawnWrapper)) {
        g_log("spawn wrapper (LogicUtilsImpl slot %d, RVA %08X) is not as expected - nothing patched",
              kSlotSpawnWrapper, img.rva(wrapper));
        return;
    }
    g_spawnCtx = reinterpret_cast<void*>(Image::ripTarget(wrapper + kSpawnCtxLea, 3, 7));
    g_spawn = reinterpret_cast<SpawnFn>(Image::ripTarget(wrapper + kSpawnCall, 1, 5));
    if (!Image::matches(reinterpret_cast<uintptr_t>(g_spawn), kPrologueSpawn)) {
        g_log("spawn function (RVA %08X) is not as expected - nothing patched",
              img.rva(reinterpret_cast<uintptr_t>(g_spawn)));
        return;
    }
    g_log("spawn: wrapper %08X function %08X context %08X", img.rva(wrapper),
          img.rva(reinterpret_cast<uintptr_t>(g_spawn)), img.rva(reinterpret_cast<uintptr_t>(g_spawnCtx)));
    if (!resolveKill(img)) {
        g_log("nothing patched");
        return;
    }

    dfd::PatchSet set(img);
    set.vslot("AiDoRechargeState::~ (slot 0)", stateVt, kSlotDtor, kPrologueDtor, &hookDtor, &g_origDtor);
    set.vslot("AiDoRechargeState tick (slot 6)", stateVt, kSlotTick, kPrologueTick, &hookTick, &g_origTick);
    if (set.apply(g_log))
        g_log("SpawnProbe active: filter '%s', squad '%s', ammo '%s', offset %.1f m, max_spawns %d, kill '%s'",
              g_set.filter.c_str(), g_set.squad.c_str(), g_set.ammo.c_str(), g_set.offset, g_set.maxSpawns,
              g_set.kill.c_str());
}

}  // namespace

// Loaded by winhttp.dll from bin\plugins under the loader lock: only open the log, read the settings and wait for
// logic.dll.
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);
    g_log.open(self, L"SpawnProbe");
    readSettings(self);
    g_log("SpawnProbe loaded, waiting for logic.dll");
    if (!g_watcher.watch(L"logic.dll", &onLogicLoaded)) g_log("cannot register DLL notification");
    return TRUE;
}
