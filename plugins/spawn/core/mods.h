// Which tables the Spawn Framework reads, and how the rows of several mods come together. The binding finds and reads
// the files (logic_dll/tables.cpp); this part only looks at their names and text, so it is tested without the game.
//
//   - The mods enabled in the game's MODS menu are the objects of its mods.json with "enabled": true, in the file's
//     order: [{"enabled": true, "folder": "UAV_Carriers"}, ...].
//   - In each mod, the vehicle tables: cars.xml, tanks.xml, and DefianceLoader's table-merge patches of them,
//     cars.<x>.patch.xml / tanks.<x>.patch.xml (any case).
//   - A vehicle in several tables takes the row read last: the tables of one mod in name order, the mods in the order
//     of mods.json.
#pragma once

#include <string>
#include <vector>

#include "config.h"

namespace spawn::core {

// The folders of the enabled mods, in the order of mods.json (its text). A minimal reader for the file the game
// writes: flat objects with "enabled" and "folder".
std::vector<std::string> enabledMods(const std::string& modsJson);

// Is a file, by its name (no folder), a vehicle table of ours to read?
bool isVehicleTable(const std::string& fileName);

// Add `rows` (one table's) to `into`, a row there with the same sysname replaced. Returns the sysnames that were
// already there with a different row - a clash between mods, for the log.
std::vector<std::string> mergeRows(SpawnTable& into, const SpawnTable& rows);

}  // namespace spawn::core
