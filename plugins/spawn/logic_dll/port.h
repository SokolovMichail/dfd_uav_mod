// The rules' view of logic.dll: LogicDllSpawnPort carries out what core::Spawner decides (spawn, kill, pack up, spend
// a kit), on the engine objects of engine.h. Also kept here: what the plugin remembers of each vehicle the rules
// know, and the vehicle's state in its F4 button for saved games - both written by the hooks (hooks.cpp).
//
// Saved games. A save keeps the abilities' timers and lengths (engine.h), and the entity ids; it does not keep the
// plugin's memory, nor the vehicles' addresses. So F4's two values carry what the rules need to go on after a load:
//   * cooldown running - the timer is the time left (the length is the cooldown, for the bar);
//   * squad out - the timer is 0 and the length is minus the squad's id. Nothing else reads F4's length (the game
//     never starts F4's timer from it, and the button reads it only while the timer runs); a minus can never be a
//     cooldown, and a float holds an id exactly (ids are ~40000, a float is exact up to 16.7 million).
// A vehicle the rules see for the first time after a load gets both back (LogicDllSpawnPort::saved) - its squad only
// if it is still alive, of the vehicle's own spawn_squad and team.
//
// F4 shows the cooldown from the same timer: game.dll's F4 button does not draw ability timers by itself (it is a
// two-state TwiceAbilityWindow, not a TimerAbilityWindow), so the game.dll part (game_dll/panel.cpp) greys it out,
// fills its progress bar and refuses presses while the timer runs.
#pragma once

#include <string>
#include <unordered_map>

#include "../core/spawner.h"

namespace spawn {

// A rules' vehicle handle is the vehicle entity itself.
inline core::Vehicle* handle(void* entity) { return static_cast<core::Vehicle*>(entity); }
inline void* entity(core::Vehicle* v) { return v; }

// What the plugin remembers of a vehicle the rules know: its world (the vehicle itself may be gone when its F4 state
// ends) and its sysname (for the log and to notice a new vehicle at a reused address).
struct Known {
    void* world;
    std::string name;
    bool showing = false;    // the cooldown is in F4's timer (cleared once when the wait is over)
    core::SquadId linked = 0;  // the squad F4's length holds
};
extern std::unordered_map<core::Vehicle*, Known> g_known;

class LogicDllSpawnPort final : public core::SpawnPort {
public:
    const core::SpawnConfig* config(core::Vehicle* v) override;
    bool spendKit(core::Vehicle* v, const std::string& ammo) override;
    core::SquadId spawn(core::Vehicle* v, const std::string& squad) override;
    bool alive(core::Vehicle* v, core::SquadId s) override;
    void kill(core::Vehicle* v, core::SquadId s) override;
    void pack(core::Vehicle* v) override;
    void event(const char* what, core::Vehicle* v, core::SquadId s) override;
    core::Saved saved(core::Vehicle* v) override;
};

extern LogicDllSpawnPort g_port;
extern core::Spawner g_spawner;

// Write the vehicle's state into F4 (see "Saved games"), every tick of a vehicle the rules know: while its cooldown
// runs, F4's timer is the rules' time left `left` (so the game's own countdown of it never drifts away), cleared once
// when the wait is over; while its squad is out, F4's length is minus the squad's id.
void showState(void* ai, Known& known, const core::VehicleState& s, float left);

// Squads that may have outlived their self-detonation (see port.cpp): checked a few seconds after it. finishDying -
// on every AiFacet update; forgetDying - on a map change.
bool anyDying();
void finishDying();
void forgetDying();

}  // namespace spawn
