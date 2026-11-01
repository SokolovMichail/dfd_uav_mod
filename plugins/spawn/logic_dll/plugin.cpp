// The Spawn Framework bound to today's logic.dll: engine objects, the port the rules use, the hooks, finding and
// patching. Overview - plugin.h; the addresses and how they were found - docs/Probe.md.

#include "plugin.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

// ---------------------------------------------------------------- engine objects

namespace {

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
// An MsvcString over a string kept alive by the caller (`keep` must outlive it).
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

// The engine functions the plugin calls.
using SpawnFn = void* (*)(const float* pos, void* world, const MsvcString* team, const MsvcString* sysname,
                          float angle, void* ctx, void*, void*, void*, void*, void*, void*, int);
using DetonateFn = void (*)(void* entity);
using PackFn = void (*)(void* entity);  // give the entity's AI the order to leave F4 mode (AiStopRechargeOrder)
using KillerFn = void* (*)(void* world);          // the world's kill manager
using KillFn = void (*)(void* killer, void* unit);  // queue a unit to be killed
SpawnFn g_spawn;
void* g_spawnCtx;
DetonateFn g_detonate;
PackFn g_pack;
KillerFn g_killer;  // optional: without them a squad inside a building or a vehicle outlives its vehicle
KillFn g_kill;

// A component's owner (a vehicle): [component+0x10] is a reference block, its object the owner.
void* ownerOf(void* component) { return component ? objectOf(field(component, 0x10)) : nullptr; }

// The entity's prototype, "scripts/species/cars.txt#Res_van" (vfunc +0x48), and its sysname - the part after '#'.
std::string sysnameOf(void* entity) {
    auto s = vcall<const MsvcString*>(entity, 0x48);
    if (!s) return {};
    std::string full(s->c_str(), s->size);
    size_t hash = full.rfind('#');
    return hash == std::string::npos ? full : full.substr(hash + 1);
}

// entity->vfunc(+0xb0): the entity's facet cache. +0x00 transform (vfunc +0x58 -> float[3]), +0x20 TeamFacet
// (vfunc +0x50 -> team id), +0x28 AiFacet (vfunc +0x1c8 -> AmmunitionDepotHelper, +0x3d0 -> a squad's soldiers,
// +0x370 - set before a self-detonation).
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
void* worldOf(void* entity) { return vcall<void*>(entity, 0x78); }

// A squad's soldiers: "is a squad" (vfunc +0x98(0x10)), then AiFacet +0x3d0 -> +0x68, a vector of entities.
std::vector<void*> soldierList(void* squad) {
    if (!vcall<bool>(squad, 0x98, 0x10)) return {};
    void* ai = aiOf(squad);
    void* members = ai ? vcall<void*>(ai, 0x3d0) : nullptr;
    if (!members) return {};
    auto v = vcall<void***>(members, 0x68);
    return std::vector<void*>(v[0], v[1]);
}
size_t soldiersOf(void* squad) { return soldierList(squad).size(); }

// The live entity with this id, with a strong reference the caller releases; nullptr - gone.
// world->vfunc(+0x10) is the entity registry, its vfunc(+0x60)(&ref, id) the lookup.
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

// One entry of a vehicle's ammunition depot, 0x48 bytes: +0 the ammunition row, +8 its sysname, +0x28 capacity,
// +0x2c rounds (what the ammo panel shows).
struct DepotEntry {
    void* ammo;
    MsvcString name;
    int32_t capacity, rounds;
    uint8_t rest[0x48 - 0x30];
};
static_assert(sizeof(DepotEntry) == 0x48, "depot entry");
struct EntryVector {
    DepotEntry *begin, *end, *cap;
};
constexpr size_t kDepotEntries = 0x48, kDepotConsume = 0xb0;  // slot 9 - the entries, slot 22 - consume(ammo, n)

// Team names by id: the rows of forces.xml in order (checked in game: a squad spawned with the name of its
// vehicle's team id gets the same id).
const char* const kTeams[] = {"player",       "resistance", "legion",          "cartel",      "abandoned", "civilian",
                              "integrators",  "marauders",  "founders",        "survivors",   "total_marauders",
                              "enemy_resistance", "spy",    "player_ally",     "total_enemy", "cartel_enemy"};
constexpr int kTeamCount = sizeof kTeams / sizeof kTeams[0];

// How far from its vehicle a squad appears (along the world X axis): drones fly, so it only keeps them out of it.
constexpr float kSpawnOffset = 8.0f;

// ---------------------------------------------------------------- the port

using core::SquadId;
inline core::Vehicle* handle(void* entity) { return static_cast<core::Vehicle*>(entity); }
inline void* entity(core::Vehicle* v) { return v; }

// What the plugin remembers of a vehicle the rules know: its world (the vehicle itself may be gone when its F4
// state ends) and its sysname (for the log and to notice a new vehicle at a reused address).
struct Known {
    void* world;
    std::string name;
    bool showing = false;  // the cooldown button is on (taken off once when the wait is over)
};

// The vehicle's abilities - the buttons of its command panel. Each has an "enabled" byte, which is also whether its
// button is there, and a cooldown: a timer the game counts down every frame and its full length. They live in a
// hash map in the AiFacet, id -> {id, timer, length, enabled}, refreshed every frame by AiFacet::update (0x129ff0).
// AiFacet slot 33 +0x108 sets an ability's timer, slot 34 +0x110 its length (30 / 31 read them).
//
// F4 (Pack, 7) keeps the cooldown in its own timer: game.dll's F4 button does not draw ability timers by itself (it
// is a two-state TwiceAbilityWindow, not a TimerAbilityWindow), so the plugin's game.dll part (panel.cpp) makes it
// grey out, fill its progress bar and refuse presses while the timer runs.
constexpr int kAbilityPack = 7;
constexpr size_t kSetAbilityTimer = 0x108, kSetAbilityLength = 0x110;

// Show the cooldown on the vehicle's AI: F4's timer at `left` seconds of `length`; left 0 - none.
void showCooldown(void* ai, float left, float length) {
    if (left > 0) vcall<void>(ai, kSetAbilityLength, kAbilityPack, length);
    vcall<void>(ai, kSetAbilityTimer, kAbilityPack, left);
}

// The vehicle's ammunition depot (its AiFacet +0x1c8) and its entry `name`; nullptr - none / not carried (a vehicle
// stores only the ammunition one of its guns fires).
void* depotOf(void* vehicle) {
    void* ai = aiOf(vehicle);
    return ai ? vcall<void*>(ai, 0x1c8) : nullptr;
}
DepotEntry* depotEntry(void* depot, const std::string& name) {
    if (!depot) return nullptr;
    auto e = vcall<EntryVector*>(depot, kDepotEntries);
    for (DepotEntry* k = e->begin; k != e->end; ++k)
        if (name == k->name.c_str()) return k;
    return nullptr;
}
std::unordered_map<core::Vehicle*, Known> g_known;
int g_events = 0;

// Squads self-detonated a moment ago. The self-detonation is a lethal hit to every soldier, and a soldier inside a
// building or a vehicle takes no hit: whoever of the squad is still there kDetonateCheckMs later goes into the
// engine's kill queue, as the mission action killAction does it (in the open that death does not fall - floating
// bodies - so it is only the second try).
struct Dying {
    void* world;
    core::SquadId squad;
    uint64_t due;
};
std::vector<Dying> g_dying;
constexpr uint64_t kDetonateCheckMs = 3000;

void finishDying() {
    uint64_t now = GetTickCount64();
    for (auto it = g_dying.begin(); it != g_dying.end();) {
        if (now < it->due) {
            ++it;
            continue;
        }
        Dying d = *it;
        it = g_dying.erase(it);
        RefBlock* rb = entityById(d.world, d.squad);
        if (!rb) continue;
        std::vector<void*> left = soldierList(rb->object);
        if (!left.empty()) {
            void* killer = g_killer(d.world);
            for (void* u : left) g_kill(killer, u);
            g_log("squad %d outlived its self-detonation (inside a building or a vehicle?): %zu soldier(s) killed",
                  d.squad, left.size());
        }
        release(rb);
    }
}

class LogicDllSpawnPort final : public core::SpawnPort {
public:
    const core::SpawnConfig* config(core::Vehicle* v) override {
        const auto& t = spawnTable();
        auto it = t.find(sysnameOf(entity(v)));
        return it == t.end() ? nullptr : &it->second;
    }

    bool spendKit(core::Vehicle* v, const std::string& ammo) override {
        void* depot = depotOf(entity(v));
        DepotEntry* k = depotEntry(depot, ammo);
        if (!k || k->rounds < 1) return false;
        vcall<void>(depot, kDepotConsume, k->ammo, 1u);
        return true;
    }

    SquadId spawn(core::Vehicle* v, const std::string& squad) override {
        void* e = entity(v);
        int team = teamOf(e);
        const float* p = positionOf(e);
        if (team < 0 || team >= kTeamCount || !p) {
            log("spawn: no team name for team id or no position", v, team);
            return 0;
        }
        float pos[3] = {p[0] + kSpawnOffset, p[1], p[2]};
        std::string teamName = kTeams[team];
        MsvcString teamStr = msvc(teamName), squadStr = msvc(squad);
        void* s = g_spawn(pos, worldOf(e), &teamStr, &squadStr, 0.0f, g_spawnCtx, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, -1);
        return s ? vcall<int>(s, 0x40) : 0;
    }

    bool alive(core::Vehicle* v, SquadId s) override {
        RefBlock* rb = entityById(knownWorld(v), s);
        bool live = rb && soldiersOf(rb->object) > 0;
        release(rb);
        return live;
    }

    void kill(core::Vehicle* v, SquadId s) override {
        RefBlock* rb = entityById(knownWorld(v), s);
        if (!rb) return;
        if (void* ai = aiOf(rb->object)) vcall<void>(ai, 0x370, true);
        g_detonate(rb->object);
        release(rb);
        if (g_killer && g_kill) g_dying.push_back({knownWorld(v), s, GetTickCount64() + kDetonateCheckMs});
    }

    // The vehicle is alive here: this runs from its own AiFacet update.
    void pack(core::Vehicle* v) override { g_pack(entity(v)); }

    void event(const char* what, core::Vehicle* v, SquadId s) override {
        if (!g_logEvents || g_events >= 200) return;
        ++g_events;
        auto it = g_known.find(v);
        g_log("%s: '%s' %p squad %d", what, it == g_known.end() ? "?" : it->second.name.c_str(),
              static_cast<void*>(v), s);
    }

private:
    // The vehicle's world as remembered (the vehicle itself may already be gone).
    static void* knownWorld(core::Vehicle* v) {
        auto it = g_known.find(v);
        return it == g_known.end() ? nullptr : it->second.world;
    }
    void log(const char* what, core::Vehicle* v, int n) {
        if (g_logEvents) g_log("%s: %p (%d)", what, static_cast<void*>(v), n);
    }
};

LogicDllSpawnPort g_port;
core::Spawner g_spawner(g_port);

// ---------------------------------------------------------------- the hooks

using DtorFn = void* (*)(void* self, unsigned flags);
using StateTickFn = void (*)(void* self);
using AiUpdateFn = void (*)(void* self, float dt);
DtorFn g_origStateDtor;
StateTickFn g_origStateTick;
AiUpdateFn g_origAiUpdate;
uintptr_t g_logicBase;

// The F4 states seen so far: the vehicle each belongs to (nullptr - not a spawning vehicle).
std::unordered_map<void*, core::Vehicle*> g_states;
// Vehicles the rules know - the only ones whose AiFacet updates go to the rules.
std::unordered_set<void*> g_tracked;

// The supply component an AiDoRechargeState runs ([state+0x20] reference block) and the vehicle it belongs to.
void* vehicleOfState(void* state) { return ownerOf(objectOf(field(state, 0x20))); }

// A vehicle has gone into F4 mode: if it spawns, remember it and let the rules know.
void deployed(void* state) {
    void* e = vehicleOfState(state);
    core::Vehicle* v = e ? handle(e) : nullptr;
    if (v) {
        std::string name = sysnameOf(e);
        auto known = g_known.find(v);
        if (known != g_known.end() && known->second.name != name) {  // another vehicle at a reused address
            g_spawner.onRemoved(v);
            g_known.erase(known);
            g_tracked.erase(e);
        }
        if (!g_port.config(v)) {
            v = nullptr;
        } else if (!g_known.count(v)) {
            g_known[v] = {worldOf(e), name};
            g_tracked.insert(e);
            void* ai = aiOf(e);
            g_log("'%s' %p: a spawning vehicle (AI facet vtable RVA %08X)", name.c_str(), e,
                  ai ? static_cast<unsigned>(*static_cast<uintptr_t*>(ai) - g_logicBase) : 0u);
        }
    }
    g_states[state] = v;
    if (v) g_spawner.onDeployed(v);
}

// AiDoRechargeState slot 6 - every tick while a vehicle is in F4 mode; the first one is the deploy.
void hookStateTick(void* state) {
    g_origStateTick(state);
    if (g_states.count(state)) return;
    guarded("F4 start", [&] { deployed(state); });
}

// AiDoRechargeState slot 0 - the deleting destructor: F4 mode is over. A vehicle that is gone with it (its supply
// component has no owner any more) is destroyed, not packed up.
void* hookStateDtor(void* state, unsigned flags) {
    auto it = g_states.find(state);
    if (it != g_states.end()) {
        core::Vehicle* v = it->second;
        g_states.erase(it);
        if (v) {
            guarded("F4 end", [&] {
                if (vehicleOfState(state) == entity(v)) {
                    g_spawner.onPacked(v);
                } else {
                    g_spawner.onRemoved(v);
                    g_tracked.erase(entity(v));
                    g_known.erase(v);
                }
            });
        }
    }
    return g_origStateDtor(state, flags);
}

// AiFacet slot 10 - update(dt), every frame for every vehicle: the rules' clock. While a vehicle's cooldown runs, F4's
// timer is set to the rules' time left (so the game's own countdown of it never drifts away), and cleared once when
// the wait is over.
void hookAiUpdate(void* ai, float dt) {
    g_origAiUpdate(ai, dt);
    if (!g_dying.empty()) guarded("kill", [] { finishDying(); });
    void* e = ownerOf(ai);
    if (!e || !g_tracked.count(e)) return;
    guarded("tick", [&] {
        core::Vehicle* v = handle(e);
        g_spawner.onTick(v, dt);
        auto known = g_known.find(v);
        const core::VehicleState* s = g_spawner.state(v);
        if (known == g_known.end() || !s) return;
        float left = g_spawner.cooldownLeft(v);
        if (left > 0) {
            showCooldown(ai, left, s->config->cooldown);
            known->second.showing = true;
        } else if (known->second.showing) {
            showCooldown(ai, 0, 0);
            known->second.showing = false;
        }
    });
}

// ---------------------------------------------------------------- finding

constexpr int kSlotDtor = 0, kSlotStateTick = 6, kSlotAiUpdate = 10, kSlotSpawnWrapper = 147;
constexpr const char* kPrologueStateDtor = "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 48 8D 05";
constexpr const char* kPrologueStateTick = "40 53 48 83 EC 20 48 8B D9 48 8B 49 20 48 85 C9";
constexpr const char* kPrologueAiUpdate = "48 89 5C 24 10 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 83 EC 70";
// LogicUtilsImpl slot 147: moves its arguments into place and calls the spawn function with a context (lea at +0x4c,
// call at +0x61).
constexpr const char* kSpawnWrapper =
    "48 83 EC 78 F3 0F 10 84 24 A8 00 00 00 33 C9 C7 44 24 60 FF FF FF FF 49 8B C1 4C 8B 8C 24 A0 00 00 00 4D 8B D0 "
    "48 89 4C 24 58 4C 8B DA 48 89 4C 24 50 4C 8B C0 48 89 4C 24 48 49 8B D2 48 89 4C 24 40 48 89 4C 24 38 48 89 4C "
    "24 30 48 8D 0D ?? ?? ?? ?? 48 89 4C 24 28 49 8B CB F3 0F 11 44 24 20 E8 ?? ?? ?? ?? 48 83 C4 78 C3";
constexpr size_t kSpawnCtxLea = 0x4c, kSpawnCall = 0x61;
constexpr const char* kPrologueSpawn = "48 8B C4 4C 89 48 20 4C 89 40 18 48 89 50 10 48 89 48 08 53 56 57 41 54";
// The self-detonate command's server side: "AiFacet->vfunc(+0x370)(true); call self-detonate(entity)".
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

struct Addr {
    uintptr_t stateVt, aiVt, utilsVt;
};

uintptr_t rvaOf(const Image& img, const void* p) { return p ? img.rva(reinterpret_cast<uintptr_t>(p)) : 0; }

// Find the vtables and the engine functions the plugin calls. true - all found and as expected.
bool resolve(const Image& img, Addr& a) {
    a.stateVt = img.vtableOf("AiDoRechargeState@Leonardo");
    a.aiVt = img.vtableOf("AiFacet@Leonardo");
    a.utilsVt = img.vtableOf("LogicUtilsImpl@Leonardo");
    g_log("vtables: AiDoRechargeState %08X AiFacet %08X LogicUtilsImpl %08X", a.stateVt ? img.rva(a.stateVt) : 0,
          a.aiVt ? img.rva(a.aiVt) : 0, a.utilsVt ? img.rva(a.utilsVt) : 0);
    if (!a.stateVt || !a.aiVt || !a.utilsVt) return false;

    uintptr_t wrapper = *reinterpret_cast<uintptr_t*>(a.utilsVt + 8 * kSlotSpawnWrapper);
    if (Image::matches(wrapper, kSpawnWrapper)) {
        g_spawnCtx = reinterpret_cast<void*>(Image::ripTarget(wrapper + kSpawnCtxLea, 3, 7));
        g_spawn = reinterpret_cast<SpawnFn>(Image::ripTarget(wrapper + kSpawnCall, 1, 5));
        if (!Image::matches(reinterpret_cast<uintptr_t>(g_spawn), kPrologueSpawn)) g_spawn = nullptr;
    }
    auto det = img.find(kSigDetonateCall);
    if (det.size() == 1) {
        g_detonate = reinterpret_cast<DetonateFn>(Image::ripTarget(det[0] + kDetonateCall, 1, 5));
        if (!Image::matches(reinterpret_cast<uintptr_t>(g_detonate), kPrologueDetonate)) g_detonate = nullptr;
    }
    auto pack = img.find(kSigPackCall);
    if (pack.size() == 1) {
        g_pack = reinterpret_cast<PackFn>(Image::ripTarget(pack[0] + kPackCall, 1, 5));
        if (!Image::matches(reinterpret_cast<uintptr_t>(g_pack), kProloguePack)) g_pack = nullptr;
    }
    auto kills = img.find(kSigKillCall);
    for (uintptr_t h : kills) {
        auto killer = reinterpret_cast<KillerFn>(Image::ripTarget(h + kKillerCall, 1, 5));
        auto kill = reinterpret_cast<KillFn>(Image::ripTarget(h + kKillCall, 1, 5));
        if ((g_killer && killer != g_killer) || (g_kill && kill != g_kill) ||
            !Image::matches(reinterpret_cast<uintptr_t>(killer), kPrologueKiller) ||
            !Image::matches(reinterpret_cast<uintptr_t>(kill), kPrologueKill)) {
            g_killer = nullptr;
            g_kill = nullptr;
            break;
        }
        g_killer = killer;
        g_kill = kill;
    }
    g_log("kill queue: manager %08X queue %08X (%zu calls)%s", rvaOf(img, (void*)g_killer), rvaOf(img, (void*)g_kill),
          kills.size(), g_kill ? "" : " - not found: a squad inside a building or a vehicle outlives its vehicle");
    auto slotOf = [&](size_t off) { return *reinterpret_cast<uintptr_t*>(a.aiVt + off); };
    bool setters = Image::matches(slotOf(kSetAbilityTimer), kPrologueAbilitySetter) &&
                   Image::matches(slotOf(kSetAbilityLength), kPrologueAbilitySetter);
    g_log("functions: spawn %08X (context %08X), self-detonate %08X (%zu calls), pack %08X (%zu calls), "
          "ability cooldown setters %08X / %08X (%s)",
          rvaOf(img, (void*)g_spawn), rvaOf(img, g_spawnCtx), rvaOf(img, (void*)g_detonate), det.size(),
          rvaOf(img, (void*)g_pack), pack.size(), img.rva(slotOf(kSetAbilityTimer)), img.rva(slotOf(kSetAbilityLength)),
          setters ? "as expected" : "NOT as expected");
    return g_spawn && g_detonate && g_pack && setters;
}

}  // namespace

bool spawnsSquads(void* entity) { return entity && spawnTable().count(sysnameOf(entity)) != 0; }

// ---------------------------------------------------------------- installing

bool install(const Image& img, Patcher& p) {
    g_logicBase = img.base();
    g_log("logic.dll at %p (TimeDateStamp %08X, SizeOfImage %08X)", reinterpret_cast<void*>(img.base()),
          img.timeDateStamp(), img.sizeOfImage());
    Addr a{};
    if (!resolve(img, a)) {
        g_log("this logic.dll build is not supported - nothing patched");
        return false;
    }
    p.vslot("AiDoRechargeState::~ (F4 end)", a.stateVt, kSlotDtor, kPrologueStateDtor, &hookStateDtor,
            &g_origStateDtor);
    p.vslot("AiDoRechargeState tick (F4 start)", a.stateVt, kSlotStateTick, kPrologueStateTick, &hookStateTick,
            &g_origStateTick);
    p.vslot("AiFacet::update (clock)", a.aiVt, kSlotAiUpdate, kPrologueAiUpdate, &hookAiUpdate, &g_origAiUpdate);
    return p.apply();
}

namespace {

// The standalone Patcher: dfd::PatchSet checks every write, then makes them all itself.
class PatchSetPatcher final : public Patcher {
public:
    explicit PatchSetPatcher(const Image& img) : set_(img) {}
    void slot(const char* what, uintptr_t vtable, int slot, const char* prologue, void* hook,
              void** saved) override {
        set_.vslot(what, vtable, slot, prologue, hook, saved);
    }
    bool apply() override { return set_.apply(g_log.file()); }

private:
    dfd::PatchSet set_;
};

}  // namespace

void onLogicLoaded(uintptr_t base) {
    Image img(base);
    PatchSetPatcher p(img);
    if (install(img, p)) g_log("Spawn Framework active");
}

void onGameLoaded(uintptr_t base) {
    Image img(base);
    PatchSetPatcher p(img);
    if (installPanel(img, p)) g_log("Spawn Framework: F4 shows the cooldown");
}

}  // namespace spawn
