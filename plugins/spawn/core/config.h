// What a vehicle spawns: the Spawn Framework's own columns in a vehicle table (cars.xml, tanks.xml).
//
//   abilities       must name deploy_unit (with the vehicle's other abilities, e.g. ammunition_recharge,deploy_unit):
//                   the switch that makes the vehicle one of the framework's, as support / repair do for HEMTT Spares
//   spawn_squad     the squad a deployed vehicle creates next to itself (squads.xml sysname); required
//   spawn_ammo      the vehicle's ammunition one round of which is spent per squad - the "kit" (ammunition.xml
//                   sysname, carried in the vehicle's `ammunition` and loaded by one of its guns); empty - no kits,
//                   the vehicle spawns without spending anything
//   spawn_cooldown  seconds of game time after its squad dies before the vehicle may spawn the next one (60)
//
// The game ignores columns it does not know, so these live in the mod's own copy of the table (or in a
// <table>.<mod>.patch.xml for DefianceLoader's table merge). Tables are Excel 2003 XML (SpreadsheetML).
#pragma once

#include <map>
#include <string>

namespace spawn::core {

struct SpawnConfig {
    std::string squad;
    std::string ammo;
    float cooldown = 60.0f;
};

// Vehicle sysname -> what it spawns.
using SpawnTable = std::map<std::string, SpawnConfig>;

constexpr float kDefaultCooldown = 60.0f;
constexpr const char* kAbility = "deploy_unit";  // the framework's name in the abilities column

// The rows of a SpreadsheetML table (its first worksheet) whose abilities name deploy_unit and that have a
// spawn_squad, added to `into` (a row there with the same sysname is replaced). A table without those columns adds
// nothing. Returns the number of rows added; malformed XML adds what was read before the problem.
int readSpawnColumns(const std::string& xml, SpawnTable& into);

}  // namespace spawn::core
