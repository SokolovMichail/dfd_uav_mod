// How a vehicle's F4 button looks: its row's pictures and tooltip keys (config.h), or the framework's defaults. The
// binding puts them on the game's widgets (game_dll/panel.cpp); the choice is made here, so it is tested without the
// game.
//
// The defaults are files of the Spawn Framework mod (mods\Spawn_Framework) and tooltip rows every mod made on it
// ships: mod/framework/framework.py makes them under the same names (DEFAULT_PREFIX, DEFAULT_TIP...) - change both.
#pragma once

#include <string>

#include "config.h"

namespace spawn::core {

// The default pictures, as layouts name them (main\buttons2\x.dds is basis\textures\ui\pictures\Main\buttons2\x.dds).
constexpr const char* kDefaultIcon = "main\\buttons2\\spawn_framework_ready.dds";
constexpr const char* kDefaultIconCooldown = "main\\buttons2\\spawn_framework_cooldown.dds";
constexpr const char* kDefaultIconRecall = "main\\buttons2\\spawn_framework_recall.dds";
// The default tooltip keys (locale/strres.xml), and the game's own, which every other vehicle keeps.
constexpr const char* kDefaultTip = "spawn_deploy";
constexpr const char* kDefaultTipRecall = "spawn_recall";
constexpr const char* kGameTip = "order_unpack";
constexpr const char* kGameTipRecall = "order_pack";
// The longest tooltip key: the binding writes a key into the game's own string in place, and a std::string keeps at
// most 15 characters in itself (more would need memory of the game's).
constexpr size_t kMaxTip = 15;

// F4's look for one vehicle.
struct Look {
    std::string icon, iconCooldown, iconRecall;  // the deploy button, what the cooldown fills it with, the pack-up one
    std::string tip, tipRecall;                  // the two buttons' tooltip keys
    // A tooltip key of the row longer than kMaxTip, replaced by the default ("" - none): for the log.
    std::string tooLong;
};

// The look of a vehicle of the framework (c), or of any other vehicle (nullptr - the game's tooltips; no pictures,
// the game draws its own).
Look lookOf(const SpawnConfig* c);

// Is `key` a tooltip key the framework sets: a default or any vehicle's of `table`? Only such a key, or the game's
// own, is ever replaced in a button - a key another mod has put there is left alone.
bool isFrameworkTip(const SpawnTable& table, const std::string& key);

}  // namespace spawn::core
