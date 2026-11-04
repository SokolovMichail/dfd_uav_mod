// The Spawn Framework bound to today's game: what the standalone DLL (standalone.cpp) and the DefianceLoader plugin
// (defiance.cpp) share. The rules are in core/ (spawner.h) and never touch the game; everything here is the adapter.
//
// The files:
//   engine.h / .cpp   the game's objects: layouts, virtual-call offsets, the engine functions and finding them
//   port.h / .cpp     the rules' view of the game (core::SpawnPort), and the vehicle's state kept in F4 for saves
//   hooks.cpp         the logic.dll hooks that feed the rules, and installing them
//   tables.cpp        reading the framework's columns from the enabled mods' tables (the parsing is core/)
//   log.cpp           the log and the crash guard
//   game_dll/panel.cpp  game.dll: F4's button shows the cooldown and each vehicle's own pictures and tooltips
//
// How it works in logic.dll (GOG 1.09.1221; the plugin finds everything by RTTI and byte patterns, and checks the
// first bytes of every function it calls or replaces - engine.h, hooks.cpp):
//  * F4 mode is the AI state AiDoRechargeState: its first tick (slot 6) - the vehicle has deployed; its destructor
//    (slot 0) - F4 mode is over, for whatever reason (packing up, the vehicle destroyed, another order).
//  * Game time: AiFacet::update(dt) (slot 10) runs every frame for every vehicle - the cooldowns run on its dt, and a
//    new world there is a map change (a mission started, a game loaded).
//  * Spawning: the function behind Lua game.spawnSquad (LogicUtilsImpl slot 147 -> 0x526d10), with the vehicle's
//    team name (the rows of forces.xml in team id order).
//  * Kits: the vehicle's AmmunitionDepotHelper (its AiFacet +0x1c8): consume one round (slot 22), as firing does.
//  * Killing: the engine's self-detonation of bomb drones (0x532f60, behind the self-detonate command): every
//    soldier takes a lethal hit and dies as if hit, with its explode_damage and destroy_effect. A soldier inside a
//    building or a vehicle takes no hit: a few seconds later it goes into the engine's kill queue (0x542360 /
//    0x292d70, as the mission action killAction does).
//  * Packing up when the squad is lost: the mission action's "pack" (0x10ef50) - an AiStopRechargeOrder to the
//    vehicle's AI, as the player's F4 gives.
//  * Saved games: the vehicle's cooldown and squad are kept in F4's own ability timer and length, which the game
//    saves (port.h, "Saved games").
//  * What a vehicle spawns: the framework's columns of the vehicle tables of the enabled mods (tables.cpp).
// In game.dll (panel.cpp): F4's button (TwiceAbilityWindow) greys out and fills a progress bar while F4's timer runs.
// docs/Probe.md has the first probe's findings; every address the plugin uses is documented where it is used.
#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "../../../common/guard.h"
#include "../../../common/patch.h"
#include "../core/spawner.h"

namespace spawn {

using dfd::Image;

// ---------------------------------------------------------------- the log

// The plugin's log. By default a file next to the DLL (bin\plugins\SpawnFramework.log); a mod loader can take the
// lines into its own log instead (sendTo). printf-style, one line per call.
class Logger {
public:
    void open(HMODULE self, const wchar_t* name) { file_.open(self, name); }
    void sendTo(void (*sink)(const char* line)) { sink_ = sink; }
    void operator()(const char* fmt, ...);
    dfd::Log& file() { return file_; }

private:
    dfd::Log file_;
    void (*sink_)(const char*) = nullptr;
};

extern Logger g_log;
// Event lines (deploys, spawns, kills): false - only install lines and faults are logged.
extern bool g_logEvents;

// ---------------------------------------------------------------- the crash guard

constexpr int kMaxFaults = 3;
extern bool g_ownWorkOff;  // too many faults: the hooks only call the game's originals
void onFault(const char* where, const dfd::Fault& fault);
// Run f, the mod's part of a hook, under the guard; `where` names it in the log.
template <class F>
void guarded(const char* where, F&& f) {
    if (g_ownWorkOff) return;
    dfd::Fault fault;
    if (dfd::guarded(f, &fault)) onFault(where, fault);
}

// ---------------------------------------------------------------- what vehicles spawn (tables.cpp)

// The framework's columns of the vehicle tables of every mod enabled in the game's MODS menu, read on first use.
const core::SpawnTable& spawnTable();
// The game folder (with bin\, mods\), where the mods are; set by the entry point before anything is read.
void setGameDir(const std::wstring& gameDir);
// The game folder: two levels above the folder the plugin DLL is in (bin\plugins\, DefianceLoader\plugins\).
std::wstring gameDirOf(HMODULE self);
// The framework's row of a vehicle entity: its sysname's in spawnTable(); nullptr - not a vehicle of the framework.
const core::SpawnConfig* spawnConfigOf(void* entity);

// ---------------------------------------------------------------- finding and patching

// Where install() sends the plugin's writes to logic.dll: the standalone DLL writes them itself, all or nothing
// (dfd::PatchSet); DefianceLoader wants to own every write.
class Patcher {
public:
    virtual ~Patcher() = default;
    // Point vtable slot `slot` at hook. The function now in the slot must start with prologue; *saved gets it.
    virtual void slot(const char* what, uintptr_t vtable, int slot, const char* prologue, void* hook,
                      void** saved) = 0;
    // Check every queued write, then make them all, or none. false - nothing written (the reason is logged).
    virtual bool apply() = 0;

    template <class Fn>
    void vslot(const char* what, uintptr_t vtable, int index, const char* prologue, Fn hook, Fn* saved) {
        slot(what, vtable, index, prologue, reinterpret_cast<void*>(hook), reinterpret_cast<void**>(saved));
    }
};

// Find everything in logic.dll and send all the plugin's writes to p. true - p accepted them all.
bool install(const Image& img, Patcher& p);
// The game.dll part (game_dll/panel.cpp): F4's button shows the cooldown. Independent of install(): without it
// spawning works, the wait is just not shown.
bool installPanel(const Image& gameImg, Patcher& p);
// The standalone way: install() / installPanel() with the writes made at once by dfd::PatchSet.
void onLogicLoaded(uintptr_t base);
void onGameLoaded(uintptr_t base);

}  // namespace spawn
