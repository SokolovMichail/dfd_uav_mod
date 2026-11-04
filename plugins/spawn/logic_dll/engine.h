// The game's objects as the plugin sees them: memory layouts, virtual-call offsets and the engine functions it calls.
// Shared by the logic.dll part (port.cpp, hooks.cpp) and the game.dll part (game_dll/panel.cpp): every offset is
// written once, here, so a game update means one place to fix.
//
// Offsets are bytes (a vtable slot n is at n * 8); addresses are RVAs in GOG 1.09.1221 logic.dll. The plugin finds
// the functions by byte patterns at load time (findEngine) and checks each one's first bytes - nothing is hard-coded
// by address. Phase 0's findings are in docs/Probe.md; the rest is documented here, where it is used.
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../../../common/patch.h"

namespace spawn::engine {

// ---------------------------------------------------------------- basics

// A virtual call: the function at byte `offset` of obj's vtable.
template <class R, class... A>
R vcall(void* obj, size_t offset, A... args) {
    auto vtbl = *reinterpret_cast<void***>(obj);
    return reinterpret_cast<R (*)(void*, A...)>(vtbl[offset / 8])(obj, args...);
}

// The pointer at byte `off` of obj.
inline void* field(void* obj, size_t off) { return *reinterpret_cast<void**>(static_cast<char*>(obj) + off); }

// The engine's reference block (its shared pointer): [+0x08] strong count, [+0x10] the object.
struct RefBlock {
    void** vtbl;
    int32_t strong, weak;
    void* object;
};
inline void* objectOf(void* refBlock) { return refBlock ? static_cast<RefBlock*>(refBlock)->object : nullptr; }
// Give back a strong reference taken from the engine, the way the engine does it: at zero, vtable +0x10 frees it.
inline void release(RefBlock* rb) {
    if (rb && --rb->strong == 0) vcall<void>(rb, 0x10);
}

// std::string as the game's MSVC runtime lays it out: up to 15 characters in buf, longer ones at ptr.
struct MsvcString {
    union {
        char buf[16];
        const char* ptr;
    };
    size_t size, cap;
    const char* c_str() const { return cap > 15 ? ptr : buf; }
};
// An MsvcString for the game to read, over a string the caller keeps alive (`keep` must outlive it). The game only
// reads it: no memory of the game's is allocated or freed.
inline MsvcString msvc(const std::string& keep) {
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

// ---------------------------------------------------------------- entities

// A component's owner (a vehicle): [component+0x10] is a reference block, its object the owner.
inline void* ownerOf(void* component) { return component ? objectOf(field(component, 0x10)) : nullptr; }
// The entity's facet cache (vfunc +0xb0): +0x00 transform (vfunc +0x58 -> float[3] position), +0x20 TeamFacet (vfunc
// +0x50 -> team id), +0x28 AiFacet (vfunc +0x1c8 -> AmmunitionDepotHelper, +0x3d0 -> a squad's soldiers, +0x370 -
// set before a self-detonation).
inline void* cacheOf(void* entity) { return vcall<void*>(entity, 0xb0); }
inline void* aiOf(void* entity) { return field(cacheOf(entity), 0x28); }
// The entity's world (vfunc +0x78): the map it is on.
inline void* worldOf(void* entity) { return vcall<void*>(entity, 0x78); }
// The entity's id (vfunc +0x40): what saved games keep, and what entityById looks up.
inline int idOf(void* entity) { return vcall<int>(entity, 0x40); }

// The entity's prototype, "scripts/species/cars.txt#Res_van" (vfunc +0x48), and its sysname - the part after '#'.
std::string sysnameOf(void* entity);
const float* positionOf(void* entity);  // nullptr - none
int teamOf(void* entity);               // -1 - none
// A squad's soldiers: "is a squad" (vfunc +0x98(0x10)), then AiFacet +0x3d0 -> +0x68, a vector of entities.
std::vector<void*> soldierList(void* squad);
// The live entity with this id, with a strong reference the caller releases; nullptr - gone.
// world->vfunc(+0x10) is the entity registry, its vfunc(+0x60)(&ref, id) the lookup.
RefBlock* entityById(void* world, int id);

// Team names by id: the rows of forces.xml in order (checked in game: a squad spawned with the name of its vehicle's
// team id gets the same id). nullptr - not a team id.
const char* teamName(int id);

// ---------------------------------------------------------------- the ammunition depot

// One entry of a vehicle's ammunition depot, 0x48 bytes: +0 the ammunition row, +8 its sysname, +0x28 capacity,
// +0x2c rounds (what the ammo panel shows).
struct DepotEntry {
    void* ammo;
    MsvcString name;
    int32_t capacity, rounds;
    uint8_t rest[0x48 - 0x30];
};
static_assert(sizeof(DepotEntry) == 0x48, "depot entry");
constexpr size_t kDepotEntries = 0x48, kDepotConsume = 0xb0;  // slot 9 - the entries, slot 22 - consume(ammo, n)

// The vehicle's ammunition depot (its AiFacet +0x1c8); nullptr - none.
void* depotOf(void* vehicle);
// The depot's entry `name`; nullptr - not carried (a vehicle stores only the ammunition one of its guns fires).
DepotEntry* depotEntry(void* depot, const std::string& name);

// ---------------------------------------------------------------- abilities

// A vehicle's abilities - the buttons of its command panel. Each has an "enabled" byte, which is also whether its
// button is there, and a cooldown: a timer the game counts down every frame and its full length. They live in a hash
// map in the AiFacet, id -> {id, timer, length, enabled}, refreshed every frame by AiFacet::update (0x129ff0).
// AiFacet slot 30 (+0xf0) reads an ability's timer, 31 (+0xf8) its length; 33 (+0x108) sets the timer, 34 (+0x110)
// the length.
constexpr int kAbilityPack = 7;  // F4: deploy / pack up
constexpr size_t kGetAbilityTimer = 0xf0, kGetAbilityLength = 0xf8, kSetAbilityTimer = 0x108,
                 kSetAbilityLength = 0x110;
inline float abilityTimer(void* ai, int id) { return vcall<float>(ai, kGetAbilityTimer, id); }
inline float abilityLength(void* ai, int id) { return vcall<float>(ai, kGetAbilityLength, id); }
inline void setAbilityTimer(void* ai, int id, float v) { vcall<void>(ai, kSetAbilityTimer, id, v); }
inline void setAbilityLength(void* ai, int id, float v) { vcall<void>(ai, kSetAbilityLength, id, v); }

// ---------------------------------------------------------------- engine functions

// The function behind Lua game.spawnSquad (0x526d10, called by LogicUtilsImpl slot 147 with a context).
using SpawnFn = void* (*)(const float* pos, void* world, const MsvcString* team, const MsvcString* sysname,
                          float angle, void* ctx, void*, void*, void*, void*, void*, void*, int);
// The bomb drones' self-detonation (0x532f60): every soldier takes a lethal hit and dies as if hit.
using DetonateFn = void (*)(void* entity);
// The mission action's "pack" (0x10ef50): the entity's AI gets the order to leave F4 mode (AiStopRechargeOrder), as
// the player's F4 gives.
using PackFn = void (*)(void* entity);
using KillerFn = void* (*)(void* world);            // the world's kill manager (0x542360)
using KillFn = void (*)(void* killer, void* unit);  // queue a unit to be killed (0x292d70), as killAction does

extern SpawnFn g_spawn;
extern void* g_spawnCtx;
extern DetonateFn g_detonate;
extern PackFn g_pack;
// Optional: without them a squad inside a building or a vehicle outlives its vehicle.
extern KillerFn g_killer;
extern KillFn g_kill;

// Find the engine functions in logic.dll and check AiFacet's ability setters (`aiVtable`; `utilsVtable` -
// LogicUtilsImpl's). true - everything required found and as expected (the kill queue is optional). Logs what it
// found.
bool findEngine(const dfd::Image& img, uintptr_t aiVtable, uintptr_t utilsVtable);

}  // namespace spawn::engine
