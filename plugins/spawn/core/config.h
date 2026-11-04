// What a vehicle spawns: the Spawn Framework's own columns in a vehicle table (cars.xml, tanks.xml).
//
//   spawn_squad     the squad a deployed vehicle creates next to itself (squads.xml sysname); required
//   spawn_ammo      the vehicle's ammunition one round of which is spent per squad - the "kit" (ammunition.xml
//                   sysname, carried in the vehicle's `ammunition` and loaded by one of its guns); empty - no kits,
//                   the vehicle spawns without spending anything
//   spawn_cooldown  seconds of game time after its squad dies before the vehicle may spawn the next one (60)
//   spawn_tethered  true (default) - the squad belongs to its vehicle: it dies with the vehicle's pack-up or
//                   destruction, and losing it packs the vehicle up; false / 0 / no - the vehicle only launches
//                   squads: each is on its own from then on, and the cooldown starts at the launch (spawner.h)
//   spawn_count     squads launched per deploy, one kit each (1, at most kMaxCount). Untethered vehicles only for
//                   now: a tethered vehicle keeps one squad (a saved game keeps the link to one squad)
//
// How the vehicle's F4 button looks - each mod brings its own; empty - the Spawn Framework mod's defaults:
//   spawn_icon            F4's deploy button, a UI picture as layouts name them (main\buttons2\x.dds); the
//                         cooldown fills over it
//   spawn_icon_cooldown   the picture the cooldown fills with (a dim copy of spawn_icon)
//   spawn_icon_recall     F4's pack-up button
//   (the defaults, and how the plugin picks between them and the row's - look.h)
//   spawn_tooltip         the deploy button's tooltip: a locale/strres.xml key, with <key>_desc and <key>_add rows;
//                         at most 15 characters (the plugin writes it into the game's string in place)
//   spawn_tooltip_recall  the pack-up button's, the same way
//
// The game ignores columns it does not know, so these live in the mod's own copy of the table (or in a
// <table>.<mod>.patch.xml for DefianceLoader's table merge). Tables are Excel 2003 XML (SpreadsheetML).
#pragma once

#include <map>
#include <string>

namespace spawn::core {

constexpr float kDefaultCooldown = 60.0f;  // spawn_cooldown empty or not a number
constexpr int kMaxCount = 20;              // spawn_count above it is taken as this

// One vehicle's row: what it spawns, and how its F4 button looks.
struct SpawnConfig {
    std::string squad;
    std::string ammo;
    float cooldown = kDefaultCooldown;
    bool tethered = true;
    int count = 1;  // as the table has it; the rules launch unitsPerDeploy()
    std::string icon, iconCooldown, iconRecall;  // empty - the default
    std::string tooltip, tooltipRecall;          // empty - the default

    // Squads a deploy launches: a tethered vehicle keeps one.
    int unitsPerDeploy() const { return tethered ? 1 : count; }

    bool operator==(const SpawnConfig&) const = default;
};

// Vehicle sysname -> what it spawns.
using SpawnTable = std::map<std::string, SpawnConfig>;

// The rows of a SpreadsheetML table (its first worksheet) that have a spawn_squad, added to `into` (a row there
// with the same sysname is replaced). A table without a spawn_squad column adds nothing. Returns the number of rows
// added; malformed XML adds what was read before the problem.
int readSpawnColumns(const std::string& xml, SpawnTable& into);

}  // namespace spawn::core
