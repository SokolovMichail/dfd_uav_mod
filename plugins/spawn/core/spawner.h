// The Spawn Framework's rules: a vehicle that is deployed (F4 mode) keeps one squad of its own next to it.
//
//   - Deployed, with no squad alive and its cooldown over, the vehicle spends one kit (spawn_ammo) and spawns its
//     squad (spawn_squad); no kit left - nothing. A vehicle without spawn_ammo spawns without spending anything.
//   - One squad alive per vehicle at a time.
//   - The squad dies when the vehicle leaves F4 mode for whatever reason (packing up, destroyed, another order).
//   - After the squad dies - killed in battle or with its vehicle's pack-up - the vehicle waits spawn_cooldown
//     seconds of game time before the next one.
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
    // Report a notable event (the binding decides how to log it).
    virtual void event(const char* what, Vehicle* v, SquadId s) = 0;
};

// What the rules remember about one vehicle.
struct VehicleState {
    const SpawnConfig* config = nullptr;  // taken from the port when the vehicle first deploys
    bool deployed = false;
    SquadId squad = 0;
    float cooldown = 0;  // seconds of game time left before the next spawn
    bool noKit = false;  // the last try found no kit (reported once until a spawn succeeds)
};

class Spawner {
public:
    explicit Spawner(SpawnPort& port) : port_(port) {}

    // The vehicle has gone into F4 mode. Not a spawning vehicle - ignored.
    void onDeployed(Vehicle* v);
    // The vehicle has left F4 mode (packed up, another order): its squad dies, the cooldown starts.
    void onPacked(Vehicle* v);
    // The vehicle is gone from the game (destroyed): its squad dies and the vehicle is forgotten - the binding may
    // reuse the handle for another vehicle.
    void onRemoved(Vehicle* v);
    // Every frame for every vehicle, with the frame's game time: cooldowns run (deployed or not), a deployed vehicle
    // notices its squad's death and spawns when it may. Vehicles the rules do not know are skipped at once.
    void onTick(Vehicle* v, float dt);

    // The state of a vehicle, nullptr - never deployed (for logs and tests).
    const VehicleState* state(Vehicle* v) const;

private:
    void trySpawn(Vehicle* v, VehicleState& s, const SpawnConfig& c);

    SpawnPort& port_;
    std::unordered_map<Vehicle*, VehicleState> vehicles_;
};

}  // namespace spawn::core
