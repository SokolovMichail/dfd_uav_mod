// The hooks in logic.dll that feed the rules (core::Spawner), and installing them. Overview - plugin.h.
//
//   AiDoRechargeState slot 6  - every tick of F4 mode; the first one is the deploy (onDeployed);
//   AiDoRechargeState slot 0  - its destructor: F4 mode is over, packed up or destroyed (onPacked / onRemoved, a frame
//                               later - see "Ended");
//   AiFacet slot 10           - update(dt), every frame for every vehicle: the rules' clock (onTick), the map change
//                               (reset), and the vehicle's state written into F4 (port.h, "Saved games").

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "engine.h"
#include "plugin.h"
#include "port.h"

namespace spawn {

using namespace engine;

namespace {

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

// F4 modes that are over, handled on the next AiFacet update of the same map: when a game is loaded, the old map's
// F4 states end while it is torn down, and nothing of it may be touched then (its squads may be gone already). A map
// change drops them.
struct Ended {
    core::Vehicle* vehicle;
    bool gone;  // destroyed, not packed up
};
std::vector<Ended> g_ended;

// The map the AiFacets update now (an entity's world). Another one - a mission started or a game was loaded: the
// rules and the plugin forget everything (no kills: the old map is gone with its squads).
void* g_world = nullptr;

void mapChanged(void* world) {
    if (g_world)
        g_log("map changed: %zu spawning vehicle(s) forgotten (a saved game keeps their squads and cooldowns in F4)",
              g_known.size());
    g_world = world;
    g_spawner.reset();
    g_known.clear();
    g_tracked.clear();
    g_states.clear();
    g_ended.clear();
    forgetDying();
}

// A vehicle has gone into F4 mode: if it spawns, remember it and let the rules know.
void deployed(void* state) {
    void* e = vehicleOfState(state);
    core::Vehicle* v = e ? handle(e) : nullptr;
    if (v && worldOf(e) != g_world) mapChanged(worldOf(e));  // an F4 state of a new map ticked before its AiFacets
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
        if (v) guarded("F4 end", [&] { g_ended.push_back({v, vehicleOfState(state) != entity(v)}); });
    }
    return g_origStateDtor(state, flags);
}

void finishEnded() {
    std::vector<Ended> ended;
    ended.swap(g_ended);
    for (const Ended& x : ended) {
        if (!x.gone) {
            g_spawner.onPacked(x.vehicle);
            continue;
        }
        g_spawner.onRemoved(x.vehicle);
        g_tracked.erase(entity(x.vehicle));
        g_known.erase(x.vehicle);
    }
}

// AiFacet slot 10 - update(dt), every frame for every vehicle: the rules' clock.
void hookAiUpdate(void* ai, float dt) {
    g_origAiUpdate(ai, dt);
    void* e = ownerOf(ai);
    if (!e) return;
    guarded("map", [&] {
        void* world = worldOf(e);
        if (world != g_world) mapChanged(world);
        if (!g_ended.empty()) finishEnded();
    });
    if (anyDying()) guarded("kill", [] { finishDying(); });
    if (!g_tracked.count(e)) return;
    guarded("tick", [&] {
        core::Vehicle* v = handle(e);
        g_spawner.onTick(v, dt);
        auto known = g_known.find(v);
        const core::VehicleState* s = g_spawner.state(v);
        if (known != g_known.end() && s) showState(ai, known->second, *s, g_spawner.cooldownLeft(v));
    });
}

// ---------------------------------------------------------------- installing

constexpr int kSlotDtor = 0, kSlotStateTick = 6, kSlotAiUpdate = 10;
constexpr const char* kPrologueStateDtor = "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 48 8D 05";
constexpr const char* kPrologueStateTick = "40 53 48 83 EC 20 48 8B D9 48 8B 49 20 48 85 C9";
constexpr const char* kPrologueAiUpdate = "48 89 5C 24 10 48 89 6C 24 20 56 57 41 54 41 56 41 57 48 83 EC 70";

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

bool install(const Image& img, Patcher& p) {
    g_logicBase = img.base();
    g_log("logic.dll at %p (TimeDateStamp %08X, SizeOfImage %08X)", reinterpret_cast<void*>(img.base()),
          img.timeDateStamp(), img.sizeOfImage());
    uintptr_t stateVt = img.vtableOf("AiDoRechargeState@Leonardo");
    uintptr_t aiVt = img.vtableOf("AiFacet@Leonardo");
    uintptr_t utilsVt = img.vtableOf("LogicUtilsImpl@Leonardo");
    g_log("vtables: AiDoRechargeState %08X AiFacet %08X LogicUtilsImpl %08X", stateVt ? img.rva(stateVt) : 0,
          aiVt ? img.rva(aiVt) : 0, utilsVt ? img.rva(utilsVt) : 0);
    if (!stateVt || !aiVt || !utilsVt || !findEngine(img, aiVt, utilsVt)) {
        g_log("this logic.dll build is not supported - nothing patched");
        return false;
    }
    p.vslot("AiDoRechargeState::~ (F4 end)", stateVt, kSlotDtor, kPrologueStateDtor, &hookStateDtor,
            &g_origStateDtor);
    p.vslot("AiDoRechargeState tick (F4 start)", stateVt, kSlotStateTick, kPrologueStateTick, &hookStateTick,
            &g_origStateTick);
    p.vslot("AiFacet::update (clock)", aiVt, kSlotAiUpdate, kPrologueAiUpdate, &hookAiUpdate, &g_origAiUpdate);
    return p.apply();
}

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
