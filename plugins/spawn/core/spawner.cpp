// The Spawn Framework's rules. Overview - spawner.h.

#include "spawner.h"

namespace spawn::core {

void Spawner::onDeployed(Vehicle* v) {
    auto it = vehicles_.find(v);
    if (it == vehicles_.end()) {
        const SpawnConfig* c = port_.config(v);
        if (!c) return;  // not ours: never remembered, so its ticks cost one lookup
        it = vehicles_.emplace(v, VehicleState{}).first;
        it->second.config = c;
        Saved saved = port_.saved(v);
        it->second.cooldown = saved.cooldown > 0 ? saved.cooldown : 0;
        if (c->tethered && saved.squad && port_.alive(v, saved.squad)) {
            it->second.squad = saved.squad;
            port_.event("squad taken back from the saved game", v, saved.squad);
        }
    }
    VehicleState& s = it->second;
    s.deployed = true;
    port_.event("deployed", v, s.squad);
    if (!s.config->tethered) {
        if (s.cooldown <= 0) launch(v, s, *s.config);
        s.packing = true;  // F4 was the launch: the vehicle packs up again, on its next tick
        return;
    }
    if (!s.squad && s.cooldown <= 0) trySpawn(v, s, *s.config);
}

void Spawner::onPacked(Vehicle* v) {
    auto it = vehicles_.find(v);
    if (it == vehicles_.end()) return;
    VehicleState& s = it->second;
    s.deployed = false;
    if (!s.squad) return;
    if (port_.alive(v, s.squad)) {
        port_.kill(v, s.squad);
        port_.event("packed up: squad killed", v, s.squad);
    } else {
        port_.event("packed up: squad already gone", v, s.squad);
    }
    s.squad = 0;
    s.cooldown = s.config->cooldown;
}

void Spawner::onRemoved(Vehicle* v) {
    onPacked(v);
    if (vehicles_.erase(v)) port_.event("vehicle gone", v, 0);
}

void Spawner::onTick(Vehicle* v, float dt) {
    auto it = vehicles_.find(v);
    if (it == vehicles_.end()) return;
    VehicleState& s = it->second;
    if (s.cooldown > 0) s.cooldown = s.cooldown > dt ? s.cooldown - dt : 0;
    if (s.packing) {  // not from the deploy itself: that runs inside F4 mode's own first tick
        s.packing = false;
        if (s.deployed) {
            s.deployed = false;
            port_.event("launched: packing up", v, 0);
            port_.pack(v);
        }
        return;
    }
    if (!s.deployed || !s.config->tethered) return;
    if (s.squad && !port_.alive(v, s.squad)) {
        port_.event("squad lost: packing up", v, s.squad);
        s.squad = 0;
        s.cooldown = s.config->cooldown;
        s.deployed = false;  // no spawn until the game ends F4 mode and the vehicle deploys again
        port_.pack(v);
        return;
    }
    if (!s.squad && s.cooldown <= 0) trySpawn(v, s, *s.config);
}

float Spawner::cooldownLeft(Vehicle* v) const {
    auto it = vehicles_.find(v);
    return it != vehicles_.end() ? it->second.cooldown : 0;
}

const VehicleState* Spawner::state(Vehicle* v) const {
    auto it = vehicles_.find(v);
    return it == vehicles_.end() ? nullptr : &it->second;
}

void Spawner::trySpawn(Vehicle* v, VehicleState& s, const SpawnConfig& c) {
    if (!c.ammo.empty() && !port_.spendKit(v, c.ammo)) {
        if (!s.noKit) port_.event("no kit left", v, 0);
        s.noKit = true;
        return;
    }
    s.noKit = false;
    s.squad = port_.spawn(v, c.squad);
    if (s.squad) {
        port_.event("squad spawned", v, s.squad);
    } else {
        // the kit is spent but nothing came out: wait the cooldown rather than spend the next kit next frame
        port_.event("spawn failed", v, 0);
        s.cooldown = c.cooldown;
    }
}

// An untethered vehicle's launch: up to unitsPerDeploy() squads, one kit each; the cooldown starts if anything was
// spent or launched (a spawn that makes nothing still costs its kit, as for a tethered vehicle).
void Spawner::launch(Vehicle* v, VehicleState& s, const SpawnConfig& c) {
    int spent = 0;
    for (int i = 0; i < c.unitsPerDeploy(); ++i) {
        if (!c.ammo.empty() && !port_.spendKit(v, c.ammo)) {
            if (spent == 0) {
                if (!s.noKit) port_.event("no kit left", v, 0);
                s.noKit = true;
            }
            break;
        }
        ++spent;
        s.noKit = false;
        SquadId q = port_.spawn(v, c.squad);
        port_.event(q ? "squad launched (untethered)" : "spawn failed", v, q);
    }
    if (spent) s.cooldown = c.cooldown;
}

}  // namespace spawn::core
