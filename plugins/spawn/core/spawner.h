// The Spawn Framework's rules: a vehicle that is deployed (F4 mode) keeps one squad of its own next to it.
//
//   - Deployed, with no squad alive and its cooldown over, the vehicle spends one kit (spawn_ammo) and spawns its
//     squad (spawn_squad); no kit left - nothing. A vehicle without spawn_ammo spawns without spending anything.
//   - One squad alive per vehicle at a time.
//   - The squad dies when the vehicle leaves F4 mode for whatever reason (packing up, destroyed, another order) -
//     unless it is recalled: packed up with the whole squad inside the vehicle's supply circle (SpawnPort::inReach),
//     the squad is taken away without a death, and its kit goes back into the vehicle - if the vehicle has room
//     for it (it may have been restocked meanwhile); no room - the kit is lost. The cooldown runs as after a loss.
//     A vehicle destroyed, or packed up because its squad was lost, recalls nothing.
//   - A squad lost in battle packs its vehicle up: the vehicle leaves F4 mode by itself.
//   - After the squad dies - killed in battle or with its vehicle's pack-up - the vehicle waits spawn_cooldown
//     seconds of game time before the next one (cooldownLeft - the binding shows it). Deployed again before the wait
//     is over, it spawns by itself when it is.
//   - Untethered vehicles (spawn_tethered false) only launch: deployed with the cooldown over, the vehicle spends a
//     kit per squad and launches spawn_count squads (as many as it has kits for), the cooldown starts at once, and
//     the vehicle packs up again by itself - F4 is its launch button. The squads are on their own from then on:
//     the rules forget them - packing up, the vehicle's destruction or the squads' deaths change nothing.
//   - Saved games: the rules forget everything when the map changes (reset). A vehicle they see for the first time
//     takes over what the game saved of it (SpawnPort::saved) - the squad it had out, if that squad is still alive,
//     and the cooldown left - instead of spawning a second squad.
//
// The core never touches game memory: it sees the game through SpawnPort, and vehicles are opaque handles.
#pragma once

#include <unordered_map>

#include "config.h"

namespace spawn::core {

// A vehicle in the game world. Opaque: only the binding knows what it points to.
class Vehicle;

// A squad, by the game's entity id; 0 - none.
using SquadId = int;

// What a saved game kept of a vehicle: the squad it had out (0 - none, or not one of its own) and the seconds of
// cooldown it had left.
struct Saved {
    SquadId squad = 0;
    float cooldown = 0;
};

// What the rules need from the game.
class SpawnPort {
public:
    virtual ~SpawnPort() = default;
    // What the vehicle spawns, by its sysname; nullptr - it is not a spawning vehicle.
    virtual const SpawnConfig* config(Vehicle* v) = 0;
    // Spend one round of `ammo` from the vehicle's own ammunition. false - none left (or the vehicle does not carry it).
    virtual bool spendKit(Vehicle* v, const std::string& ammo) = 0;
    // Spawn `squad` next to the vehicle, of the vehicle's team. 0 - the game made nothing.
    virtual SquadId spawn(Vehicle* v, const std::string& squad) = 0;
    // Is the squad still in the game with soldiers alive?
    virtual bool alive(Vehicle* v, SquadId s) = 0;
    // Kill the squad (its soldiers die as if hit).
    virtual void kill(Vehicle* v, SquadId s) = 0;
    // Is every soldier of the squad inside the vehicle's recall circle (its supply circle)?
    virtual bool inReach(Vehicle* v, SquadId s) = 0;
    // Take the squad away without a death (a recall).
    virtual void recall(Vehicle* v, SquadId s) = 0;
    // Give one round of `ammo` back to the vehicle. false - no room for it (or the vehicle does not carry it).
    virtual bool returnKit(Vehicle* v, const std::string& ammo) = 0;
    // Order the vehicle out of F4 mode, as the player's F4 does. The game ends F4 mode later (onPacked follows).
    virtual void pack(Vehicle* v) = 0;
    // Report a notable event (the binding decides how to log it).
    virtual void event(const char* what, Vehicle* v, SquadId s) = 0;
    // What the game has saved of a vehicle the rules see for the first time (nothing, in a game not loaded).
    virtual Saved saved(Vehicle* v) = 0;
};

// What the rules remember about one vehicle.
struct VehicleState {
    const SpawnConfig* config = nullptr;  // taken from the port when the vehicle first deploys
    bool deployed = false;  // in F4 mode and spawning (false already while a pack-up the rules ordered is under way)
    SquadId squad = 0;
    float cooldown = 0;  // seconds of game time left before the next spawn
    bool noKit = false;  // the last try found no kit (reported once until a spawn succeeds)
    bool packing = false;  // untethered: deployed and launched - the pack-up is ordered on the next tick
};

class Spawner {
public:
    explicit Spawner(SpawnPort& port) : port_(port) {}

    // The vehicle has gone into F4 mode. Not a spawning vehicle - ignored.
    void onDeployed(Vehicle* v);
    // The vehicle has left F4 mode (packed up, another order): its squad is recalled if in reach, or dies; the
    // cooldown starts.
    void onPacked(Vehicle* v) { end(v, true); }
    // The vehicle is gone from the game (destroyed): its squad dies and the vehicle is forgotten - the binding may
    // reuse the handle for another vehicle.
    void onRemoved(Vehicle* v);
    // Every frame for every vehicle, with the frame's game time: cooldowns run (deployed or not), a deployed vehicle
    // notices its squad's death and spawns when it may. Vehicles the rules do not know are skipped at once.
    void onTick(Vehicle* v, float dt);
    // The map has changed (a mission started, a game was loaded): every vehicle is forgotten, with no kills - the
    // squads of the old map are gone with it.
    void reset() { vehicles_.clear(); }

    // Seconds of the vehicle's cooldown still to wait, deployed or not; 0 - none (or a vehicle the rules do not know).
    float cooldownLeft(Vehicle* v) const;

    // The state of a vehicle, nullptr - never deployed (for logs and tests).
    const VehicleState* state(Vehicle* v) const;

private:
    void end(Vehicle* v, bool mayRecall);
    void trySpawn(Vehicle* v, VehicleState& s, const SpawnConfig& c);
    void launch(Vehicle* v, VehicleState& s, const SpawnConfig& c);

    SpawnPort& port_;
    std::unordered_map<Vehicle*, VehicleState> vehicles_;
};

}  // namespace spawn::core
