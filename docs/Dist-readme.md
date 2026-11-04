{IF FRAMEWORK}
Spawn Framework {VERSION} - for Terminator: Dark Fate - Defiance
=============================================================
{END}
{IF EXAMPLE}
UAV Carriers {VERSION} - mod for Terminator: Dark Fate - Defiance
=============================================================
{END}

## Description

{IF EXAMPLE}
Deploy a carrier with F4, and it launches a recon drone that you control like any other unit: eyes anywhere on
the map, with no soldier at risk. The drone belongs to its carrier: pack the carrier up, or lose it, and the drone
self-destructs; lose the drone, and the carrier packs up by itself.

* Recon UAV Carrier - the Integrators' gun truck refitted to carry a recon drone, a little slower than the
  Legion's swarm scouts.
* Patchwork UAV Carrier - a Resistance van with a drone put together from Legion scrap: slow, rusty, its eye
  glowing green.
* Each carries 5 UAV kits (500 supply to restock each, like any ammunition) and spends one per drone. One drone
  per carrier at a time; the next one is ready 60 s after the last one is gone. The carrier cannot move while it
  flies its drone, and takes no passengers.
* Where to get them:
  * Resistance campaign: both in the first sector shop.
  * Main campaign and Evolution campaign: both in every shop.
  * Skirmish: both are available to the Resistance and the Founders.

The mechanics come from the Spawn Framework, included in this package.
{END}
{IF FRAMEWORK}
The Spawn Framework lets a vehicle send out units of its own when it is deployed (F4), for any vehicle a mod
gives it to:

* Tethered units: a deployed vehicle keeps one unit of its own (a drone, or any squad) next to it. The unit dies
  with the vehicle's pack-up or destruction; losing it packs the vehicle up.
* Untethered launches: a vehicle that only sends units out - a set number per deploy, each on its own from then
  on - and packs up again by itself.
* Kits and cooldowns: each unit costs one round of the vehicle's own ammunition (the "kit"), and the next one
  waits a cooldown. F4 shows the wait: greyed out and filling up, it cannot be pressed until the wait is over.
* Each vehicle brings its own F4 pictures and tooltips. Other vehicles' F4 stays as the game draws it.
* Saved games keep it all: a vehicle comes back from a save with its own unit and its cooldown.

This package adds NO vehicles by itself. It is for mods built on it, such as UAV Carriers (available as a separate
download on the same page, with the framework included).
{END}

Making your own vehicles on the framework: see SpawnFramework_MODDING.txt.


## REQUIREMENTS
* Terminator: Dark Fate - Defiance 1.09.1221 or newer, Windows 10/11 64-bit.
  Tested on the GOG version. The Steam version should work too (see CHECK below).
* Resistance DLC optional.
* Evolution DLC optional.
{IF DEFIANCE}
* DefianceLoader 1.9.0 or newer, installed in the game folder:
  https://github.com/DefianceLoader/DefianceLoader
{END}


## INSTALLATION
"Game folder" is the folder that contains "bin" and "mods" (for GOG, for example,
C:\Games\Terminator Dark Fate - Defiance).

Before you start:
* Close the game.
* Updating from an older version of this mod? Delete the old version's folders first (mods\Spawn_Framework{IF EXAMPLE},
  mods\UAV_Carriers{END}), so no files of it stay behind. (Your saves are not in those folders.)
* Install only ONE build: standalone OR DefianceLoader, never both. To switch, uninstall the other build first
  (see UNINSTALL).

{IF DEFIANCE}
1. Install DefianceLoader first, following its own instructions, and make sure the game starts with it.
2. Extract the archive into the game folder. Allow it to replace files if asked.
3. Check that you now have:
     mods\Spawn_Framework\...
{IF EXAMPLE}
     mods\UAV_Carriers\...
{END}
     DefianceLoader\plugins\spawn_framework.dll
     DefianceLoader\plugins\spawn_framework.plugin.json
   and that you do NOT have bin\plugins\SpawnFramework.dll (the standalone build). If you do, delete it and
   bin\winhttp.dll - unless bin\winhttp.dll belongs to Dark Fate Trainer; then keep that one.
{END}
{IF STANDALONE}
1. Extract the archive into the game folder. Allow it to replace files if asked.
   Using Dark Fate Trainer? See "Standalone build and Dark Fate Trainer" below first.
2. Check that you now have:
     mods\Spawn_Framework\...
{IF EXAMPLE}
     mods\UAV_Carriers\...
{END}
     bin\winhttp.dll
     bin\plugins\SpawnFramework.dll
{END}
{IF EXAMPLE}
*. Start the game, open MODS and enable BOTH "Spawn Framework" (the F4 buttons) and "UAV Carriers" (the carriers).
*. Start a new mission (or a new campaign to get the shop rewards).
   Loading a save made after a mission works too - the next shop will have the carriers.
{END}
{IF FRAMEWORK}
*. Start the game, open MODS and enable "Spawn Framework", together with the mods that use it.
{END}
*. Check that the plugin runs (see CHECK).
{IF DEFIANCE}
This build does not use bin\winhttp.dll, so it does not touch Dark Fate Trainer's files.
{END}
{IF STANDALONE}

### Standalone build and Dark Fate Trainer
Since v1.1.3 the trainer and this mod work together. Both use bin\winhttp.dll, and the trainer's copy loads this
mod's plugin too, so you need only one of them - the trainer's.
Option A. You have neither installed yet.
  Install this mod first, then the trainer. Let the trainer replace files.
Option B. You have this mod installed and want to add the trainer.
  Install the trainer. Let it replace files.
Option C. You have the trainer installed and want to add this mod.
  1. Extract the archive somewhere else, not into the game folder. You get the folders bin and mods, and the
     README files.
  2. Open that bin folder and delete winhttp.dll there - this keeps the trainer's copy in the game.
  3. Copy the bin and mods folders into the game folder. Allow it to merge folders.
  4. Check that you now have bin\plugins\SpawnFramework.dll and bin\winhttp.dll (the trainer's), and continue
     from the "Start the game, open MODS" step.
{END}


## CHECK
After the game has started, open the log:
{IF STANDALONE}
* bin\plugins\SpawnFramework.log. It should have the lines "Spawn Framework active" and
  "Spawn Framework: F4 shows the cooldown".
{END}
{IF DEFIANCE}
* DefianceLoader\logs\defiance-loader.log. Look for the lines of sokolov.spawn-framework:
  "Spawn Framework staged", and the plugin listed as active.
{END}
At the first F4 of any vehicle the log also lists the vehicles of the framework it found{IF EXAMPLE}, for example
"Res_patchwork_uav_carrier -> Res_patchwork_uav, tethered (kit uav_kit, cooldown 60 s)"{END}.
If it says "this logic.dll build is not supported", your game build differs: the vehicles deploy, but send out
no units. If only "this game.dll build is not supported" shows, the units work, and F4 just does not show the
cooldown. Please report it together with the log.
{IF DEFIANCE}
Settings: section [sokolov.spawn-framework] in DefianceLoader\config\settings.ini ("enabled = false" turns the
plugin off, "log = false" keeps its events out of the log).
{END}


## WHAT THE DLLs DO
Launching and tying units to vehicles needs a small native plugin, because the game engine cannot do it out of
the box. No game file on disk is modified: the plugin changes the game's logic.dll and game.dll IN MEMORY while
the game runs (logic.dll: the units, kits and cooldowns; game.dll: the F4 button). Before changing anything it
checks that the game code is exactly what it expects; otherwise it changes nothing.
{IF STANDALONE}
bin\winhttp.dll only forwards everything to the Windows winhttp.dll and loads bin\plugins\*.dll;
SpawnFramework.dll does the rest.
{END}
{IF DEFIANCE}
spawn_framework.dll asks DefianceLoader to make the changes, and the loader checks that no other plugin changes
the same places.
{END}
The plugin files' checksums are in SHA256SUMS.txt on the release page.


## NOTES
* Single player only. Do not use it in multiplayer.{IF DEFIANCE} The DefianceLoader build blocks going online
  while the plugin is enabled.{END}
* Game updates may change the game files the mod relies on; then the mod needs an update too.
{IF EXAMPLE}
* A drone's self-destruction is a small blast: it also hits whatever stands right next to the drone when its
  carrier packs up.
{END}


## KNOWN ISSUES
{IF EXAMPLE}
* Both carriers use the models of the vehicles they are built on (the Integrators' gun truck and the Resistance
  van).
* A deployed carrier shows the game's ammunition supply circle, though it hands out nothing and its drone has no
  range limit.
{END}
* A unit inside a building or a vehicle when its vehicle packs up dies a few seconds later{IF EXAMPLE}, with its
  blast inside{END}.


## KNOWN MOD CONFLICTS
Any mod that replaces the same game files.
* The framework: scripts/ui/AbilityButtons/pack.txt (the F4 button).
{IF EXAMPLE}
* UAV Carriers: scripts/species/cars.xml, humans.xml, squads.xml, ammunition.xml, guns.xml, gun_mounts.xml,
  shop_presets.xml, encyclopedia.xml, encyclopedia_dlc.xml, scripts/default_army.toml, locale/cars.xml,
  locale/squads.xml, locale/strres.xml, locale/ammunition.xml, locale/encyclopedia_loc.xml,
  locale/encyclopedia_dlc_loc.xml.
  HEMTT Spares replaces several of the same tables: enable only one of the two.
{END}
{IF STANDALONE}
* Other tools that also use bin\winhttp.dll (Dark Fate Trainer works together with it - see INSTALLATION).
{END}


## UNINSTALL
Disable the mods in MODS, then delete mods\Spawn_Framework{IF EXAMPLE} and mods\UAV_Carriers{END}, and the plugin:
{IF STANDALONE}
bin\winhttp.dll and bin\plugins\SpawnFramework.dll (and bin\plugins, if nothing else is in it). Keep
bin\winhttp.dll if it is Dark Fate Trainer's.
{END}
{IF DEFIANCE}
spawn_framework.dll and spawn_framework.plugin.json in DefianceLoader\plugins.
{END}
{IF EXAMPLE}
Campaign saves that already contain a UAV carrier may fail to load without the mod.
{END}


## THANKS
* Anthropic Claude, for helping me create this mod.
* My family, for endless support.
* Gutwormy, the creator of Dark Fate Trainer, whose mod inspired how these mod files are organised.

Terminator: Dark Fate - Defiance and its DLC belong to Slitherine Ltd.
