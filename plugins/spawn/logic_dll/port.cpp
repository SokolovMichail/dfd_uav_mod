// The rules' view of logic.dll. Overview - port.h; the engine objects - engine.h.

#include "port.h"

#include <vector>

#include "engine.h"
#include "plugin.h"

namespace spawn {

using namespace engine;
using core::SquadId;

std::unordered_map<core::Vehicle*, Known> g_known;
LogicDllSpawnPort g_port;
core::Spawner g_spawner(g_port);

namespace {

// How far from its vehicle a squad appears (along the world X axis): drones fly, so it only keeps them out of it.
constexpr float kSpawnOffset = 8.0f;
// Event lines per game run: enough for a session's deploys, not a log that grows without end.
constexpr int kMaxEvents = 200;
int g_events = 0;

// The vehicle's world as remembered (the vehicle itself may already be gone).
void* knownWorld(core::Vehicle* v) {
    auto it = g_known.find(v);
    return it == g_known.end() ? nullptr : it->second.world;
}

// Squads self-detonated a moment ago. The self-detonation is a lethal hit to every soldier, and a soldier inside a
// building or a vehicle takes no hit: whoever of the squad is still there kDetonateCheckMs later goes into the
// engine's kill queue, as the mission action killAction does it (in the open that death does not fall - floating
// bodies - so it is only the second try).
struct Dying {
    void* world;
    SquadId squad;
    uint64_t due;
};
std::vector<Dying> g_dying;
constexpr uint64_t kDetonateCheckMs = 3000;

}  // namespace

// ---------------------------------------------------------------- the port

const core::SpawnConfig* LogicDllSpawnPort::config(core::Vehicle* v) { return spawnConfigOf(entity(v)); }

// One round, consumed the way firing does (depot slot 22).
bool LogicDllSpawnPort::spendKit(core::Vehicle* v, const std::string& ammo) {
    void* depot = depotOf(entity(v));
    DepotEntry* k = depotEntry(depot, ammo);
    if (!k || k->rounds < 1) return false;
    vcall<void>(depot, kDepotConsume, k->ammo, 1u);
    return true;
}

// Through the engine's spawn function, with the name of the vehicle's team, kSpawnOffset away from the vehicle.
SquadId LogicDllSpawnPort::spawn(core::Vehicle* v, const std::string& squad) {
    void* e = entity(v);
    int team = teamOf(e);
    const char* teamStr = teamName(team);
    const float* p = positionOf(e);
    if (!teamStr || !p) {
        if (g_logEvents) g_log("spawn: no team name for team id %d or no position: %p", team, static_cast<void*>(v));
        return 0;
    }
    float pos[3] = {p[0] + kSpawnOffset, p[1], p[2]};
    std::string teamKeep = teamStr;
    MsvcString teamArg = msvc(teamKeep), squadArg = msvc(squad);
    void* s = g_spawn(pos, worldOf(e), &teamArg, &squadArg, 0.0f, g_spawnCtx, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, -1);
    return s ? idOf(s) : 0;
}

bool LogicDllSpawnPort::alive(core::Vehicle* v, SquadId s) {
    RefBlock* rb = entityById(knownWorld(v), s);
    bool live = rb && !soldierList(rb->object).empty();
    release(rb);
    return live;
}

// The self-detonation (AiFacet +0x370 first, as the self-detonate command does), and a check a few seconds later.
void LogicDllSpawnPort::kill(core::Vehicle* v, SquadId s) {
    RefBlock* rb = entityById(knownWorld(v), s);
    if (!rb) return;
    if (void* ai = aiOf(rb->object)) vcall<void>(ai, 0x370, true);
    g_detonate(rb->object);
    release(rb);
    if (g_killer && g_kill) g_dying.push_back({knownWorld(v), s, GetTickCount64() + kDetonateCheckMs});
}

// The vehicle is alive here: this runs from its own AiFacet update.
void LogicDllSpawnPort::pack(core::Vehicle* v) { g_pack(entity(v)); }

void LogicDllSpawnPort::event(const char* what, core::Vehicle* v, SquadId s) {
    if (!g_logEvents || g_events >= kMaxEvents) return;
    ++g_events;
    auto it = g_known.find(v);
    g_log("%s: '%s' %p squad %d", what, it == g_known.end() ? "?" : it->second.name.c_str(), static_cast<void*>(v), s);
}

// F4's timer and length, as the save kept them (see "Saved games").
core::Saved LogicDllSpawnPort::saved(core::Vehicle* v) {
    core::Saved out;
    void* e = entity(v);
    void* ai = aiOf(e);
    if (!ai) return out;
    float timer = abilityTimer(ai, kAbilityPack);
    float length = abilityLength(ai, kAbilityPack);
    if (timer > 0) out.cooldown = timer;
    if (length > -0.5f) return out;  // no squad id in it
    auto id = static_cast<SquadId>(-length + 0.5f);
    const core::SpawnConfig* c = config(v);
    RefBlock* rb = entityById(knownWorld(v), id);
    if (rb && c && sysnameOf(rb->object) == c->squad && teamOf(rb->object) == teamOf(e)) out.squad = id;
    if (g_logEvents)
        g_log("saved game: '%s' %p - F4 timer %.1f, squad %d%s", sysnameOf(e).c_str(), e, timer, id,
              out.squad ? "" : " (not its own any more)");
    release(rb);
    return out;
}

// ---------------------------------------------------------------- the state in F4

void showState(void* ai, Known& known, const core::VehicleState& s, float left) {
    if (left > 0) {
        setAbilityLength(ai, kAbilityPack, s.config->cooldown);
        setAbilityTimer(ai, kAbilityPack, left);
        known.showing = true;
        known.linked = 0;
    } else if (known.showing) {
        setAbilityTimer(ai, kAbilityPack, 0);
        known.showing = false;
    }
    if (s.squad && known.linked != s.squad) {
        setAbilityLength(ai, kAbilityPack, -static_cast<float>(s.squad));
        known.linked = s.squad;
    }
}

// ---------------------------------------------------------------- squads that outlive their self-detonation

bool anyDying() { return !g_dying.empty(); }

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

void forgetDying() { g_dying.clear(); }

}  // namespace spawn
