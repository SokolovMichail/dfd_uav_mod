# Short description
Deploy a carrier with F4 and it launches a recon drone you fly like any other unit: eyes anywhere, no soldier at risk. Two carriers: the Integrators' Recon UAV Carrier and the Resistance's rusty Patchwork UAV Carrier. Built on the Spawn Framework, which lets any mod give a vehicle units of its own on deploy.

# Description
UAV carriers for Terminator: Dark Fate - Defiance: deploy the vehicle and it keeps a recon drone of its own in the air.

Deploy a carrier with F4, and it launches a drone that you control like any other unit. The drone belongs to its carrier: pack the carrier up, or lose it, and the drone self-destructs; lose the drone, and the carrier packs up by itself. The game engine has no such mechanics, so a small native plugin adds them. The page offers them in two parts:

**Spawn Framework** - the mechanics themselves, for any vehicle a mod gives them to:
* Tethered units: a deployed vehicle keeps one unit of its own (a drone, or any squad) next to it. The unit dies with the vehicle's pack-up or destruction; losing it packs the vehicle up.
* Untethered launches: a vehicle that only sends units out - a set number per deploy, each on its own from then on - and packs up again by itself.
* Kits and cooldowns: each unit costs one round of the vehicle's own ammunition (the "kit"), and the next one waits a cooldown. F4 shows the wait: greyed out and filling up, it cannot be pressed until the wait is over.
* Each vehicle brings its own F4 pictures and tooltips. Other vehicles' F4 stays as the game draws it.
* Saved games keep it all: a carrier comes back from a save with its own drone and its cooldown.
* Modders need no code: a few cells in the game's vehicle table make a carrier. A guide for modders is included (SpawnFramework_MODDING.txt).

**UAV Carriers** - the example mod, two carriers built on the framework:
* Recon UAV Carrier - the Integrators' gun truck refitted to carry a recon drone, a little slower than the Legion's swarm scouts.
* Patchwork UAV Carrier - a Resistance van with a drone put together from Legion scrap: slow, rusty, its eye glowing green. Flies. Sometimes.

  *Patchwork UAV - operating instructions:*
  1. Deploy the van.
  2. Recite the Tech-Litany.
  3. Pray to the Omnissiah.
  4. If the drone takes off, the blessing is granted. If not, repeat step 2 louder.

  *Warranty void for drones that revert to Legion factory settings.*
* Each carries 5 UAV kits (500 supply to restock each, like any ammunition) and spends one per drone: a drone is a pricey, single-use, fragile scout with no range limit - eyes anywhere without risking anyone, while its carrier stands still. Both drones have encyclopedia articles in the base game and both DLCs. One drone per carrier at a time; the next one is ready 60 s after the last one dies. The carriers take no passengers.
* Where to get them:
  * Resistance campaign: both in the first sector shop.
  * Main campaign and Evolution campaign: both in every shop.
  * Skirmish: both are available to the Resistance and the Founders.

**Four downloads - pick one:**
* UAV_Carriers - the framework and the two carriers. Take this one to play.
* SpawnFramework - the framework only, for making your own vehicles.
* Each comes in two builds: standalone (its own small loader, bin\winhttp.dll), or as a plugin for DefianceLoader (files with "_DefianceLoader" in the name). Install only one of the two builds.
# REQUIREMENTS
* Terminator: Dark Fate - Defiance 1.09.1221 or newer, Windows 10/11 64-bit.
    Tested on the GOG version. The Steam version should work too (see CHECK below).
* Resistance DLC optional.
* Evolution DLC optional.
* DefianceLoader builds only: DefianceLoader 1.9.0 or newer, installed in the game folder:
    https://github.com/DefianceLoader/DefianceLoader
# Installation instructions
   Pick ONE of the four archives below and follow its steps only.
   "Game folder" is the folder that contains "bin" and "mods" (for GOG, for example,
   C:\Games\Terminator Dark Fate - Defiance).

   Before you start, for any archive:
   * Close the game.
   * Updating from an older version of this mod? Delete the old mods\UAV_Carriers and mods\Spawn_Framework
     folders first, so no files of the old version stay behind. (Your saves are not in those folders.)
   * Install only ONE build: standalone OR DefianceLoader, never both. With both installed, the
     DefianceLoader plugin stays off and the standalone one runs. To switch, uninstall the other build first
     (see UNINSTALL).

   --- 1. UAV_Carriers_v1.0.zip - the full mod, standalone (recommended if you don't use DefianceLoader) ---
   1. Extract the archive into the game folder. Allow it to replace files if asked.
   2. Check that you now have:
        mods\Spawn_Framework\...
        mods\UAV_Carriers\...
        bin\winhttp.dll
        bin\plugins\SpawnFramework.dll
   3. Start the game, open MODS and enable BOTH "Spawn Framework" (the F4 buttons) and "UAV Carriers"
      (the carriers).
   4. Start a new mission (or a new campaign to get the shop rewards).
      Loading a save made after a mission works too - the next shop will have the carriers.
   5. Check that the plugin runs (see CHECK).
   Using Dark Fate Trainer? See "Standalone build and Dark Fate Trainer" below before step 1.

   --- 2. UAV_Carriers_DefianceLoader_v1.0.zip - the full mod, as a DefianceLoader plugin ---
   1. Install DefianceLoader first, following its own instructions, and make sure the game starts with it.
   2. Extract the archive into the game folder. Allow it to replace files if asked.
   3. Check that you now have:
        mods\Spawn_Framework\...
        mods\UAV_Carriers\...
        DefianceLoader\plugins\spawn_framework.dll
        DefianceLoader\plugins\spawn_framework.plugin.json
      and that you do NOT have bin\plugins\SpawnFramework.dll (the standalone build). If you do, delete it
      and bin\winhttp.dll - unless bin\winhttp.dll belongs to Dark Fate Trainer; then keep that one.
   4. Start the game, open MODS and enable BOTH "Spawn Framework" (the F4 buttons) and "UAV Carriers"
      (the carriers).
   5. Start a new mission (or a new campaign to get the shop rewards).
      Loading a save made after a mission works too - the next shop will have the carriers.
   6. Check that the plugin runs (see CHECK).
   This build does not use bin\winhttp.dll, so it does not touch Dark Fate Trainer's files.

   --- 3. SpawnFramework_v1.0.zip - the framework only, standalone ---
   For mod makers, or for a mod built on the framework that asks you to install it. Adds no vehicles.
   1. Extract the archive into the game folder. Allow it to replace files if asked.
   2. Check that you now have:
        mods\Spawn_Framework\...
        bin\winhttp.dll
        bin\plugins\SpawnFramework.dll
   3. Start the game, open MODS and enable "Spawn Framework", together with the mods that use it.
   4. Check that the plugin runs (see CHECK).
   Using Dark Fate Trainer? See "Standalone build and Dark Fate Trainer" below before step 1.

   --- 4. SpawnFramework_DefianceLoader_v1.0.zip - the framework only, as a DefianceLoader plugin ---
   For mod makers, or for a mod built on the framework that asks you to install it. Adds no vehicles.
   1. Install DefianceLoader first, following its own instructions, and make sure the game starts with it.
   2. Extract the archive into the game folder. Allow it to replace files if asked.
   3. Check that you now have:
        mods\Spawn_Framework\...
        DefianceLoader\plugins\spawn_framework.dll
        DefianceLoader\plugins\spawn_framework.plugin.json
      and that you do NOT have bin\plugins\SpawnFramework.dll (see archive 2, step 3).
   4. Start the game, open MODS and enable "Spawn Framework", together with the mods that use it.
   5. Check that the plugin runs (see CHECK).

   --- Standalone build and Dark Fate Trainer (archives 1 and 3 only) ---
   Since v1.1.3 the trainer and this mod work together. Both use bin\winhttp.dll, and the trainer's copy
   loads this mod's plugin too, so you need only one of them - the trainer's. (The trainer's page has
   installation instructions as well.)
   Option A. You have neither installed yet.
     Install this mod first, then the trainer. Let the trainer replace files.
   Option B. You have this mod installed and want to add the trainer.
     Install the trainer. Let it replace files.
   Option C. You have the trainer installed and want to add this mod.
     1. Extract the archive somewhere else, not into the game folder. You get the folders bin and mods,
        and the README files.
     2. Open that bin folder and delete winhttp.dll there - this keeps the trainer's copy in the game.
     3. Copy the bin and mods folders into the game folder. Allow it to merge folders.
     4. Check that you now have:
          mods\Spawn_Framework\...
          mods\UAV_Carriers\... (archive 1 only)
          bin\winhttp.dll (the trainer's, not mine)
          bin\plugins\SpawnFramework.dll
     5. Continue from the "Start the game, open MODS" step of your archive.
# CHECK
   After the game has started, open the log:
   * Standalone build: bin\plugins\SpawnFramework.log. It should have the lines "Spawn Framework active" and
     "Spawn Framework: F4 shows the cooldown".
   * DefianceLoader build: DefianceLoader\logs\defiance-loader.log. Look for the lines of
     sokolov.spawn-framework: "Spawn Framework staged", and the plugin listed as active.
   At the first F4 of any vehicle the log also lists the carriers it found, for example
   "Res_patchwork_uav_carrier -> Res_patchwork_uav, tethered (kit uav_kit, cooldown 60 s)".
   If it says "this logic.dll build is not supported", your game build differs: the carriers deploy, but
   launch no drone. If only "this game.dll build is not supported" shows, the drones work, and F4 just
   does not show the cooldown.
   Please report it together with the log.
   DefianceLoader build settings: section [sokolov.spawn-framework] in DefianceLoader\config\settings.ini
   ("enabled = false" turns the plugin off, "log = false" keeps its events out of the log).
# WHAT THE DLLs DO
   Launching and tying units to vehicles needs a small native plugin, because the game engine cannot do it out of
   the box.
   No game file on disk is modified: the plugin changes the game's logic.dll and game.dll IN MEMORY while the
   game runs (logic.dll: the drones, kits and cooldowns; game.dll: the F4 button). Before changing anything it
   checks that the game code is exactly what it expects; otherwise it changes nothing.
   * Standalone build: bin\winhttp.dll only forwards everything to the Windows winhttp.dll and loads
     bin\plugins\*.dll; SpawnFramework.dll does the rest.
   * DefianceLoader build: spawn_framework.dll asks DefianceLoader to make the changes, and the loader checks
     that no other plugin changes the same places.
# NOTES
* Single player only. Do not use it in multiplayer. The DefianceLoader build blocks going online while the plugin is enabled.
* Game updates may change the game files the mod relies on; then the mod needs an update too.
* A drone's self-destruction is a small blast: it also hits whatever stands right next to the drone when its carrier packs up.

# KNOWN ISSUES
* Both carriers use the models of the vehicles they are built on (the Integrators' gun truck and the Resistance van).
* A drone inside a building or a vehicle when its carrier packs up dies a few seconds later, with its blast inside.
* A deployed carrier shows the game's ammunition supply circle, though it hands out nothing and its drone has no range limit.
* 
# KNOWN MOD CONFLICTS
  HEMTT Spares (both replace scripts/species/cars.xml and other tables: enable only one of them).
  Generally, any mod that replaces the same game files.
    The framework: scripts/ui/AbilityButtons/pack.txt (the F4 button).
    UAV Carriers: scripts/species/cars.xml, humans.xml, squads.xml, ammunition.xml, guns.xml, gun_mounts.xml,
    shop_presets.xml, encyclopedia.xml, encyclopedia_dlc.xml, scripts/default_army.toml, locale/cars.xml,
    locale/squads.xml, locale/strres.xml, locale/ammunition.xml, locale/encyclopedia_loc.xml,
    locale/encyclopedia_dlc_loc.xml.
    Standalone build: other tools that also use bin\winhttp.dll.
# UNINSTALL
  Disable the mods in MODS, then delete mods\Spawn_Framework and mods\UAV_Carriers, and the plugin:
  * standalone build: bin\winhttp.dll and bin\plugins\SpawnFramework.dll (and bin\plugins, if nothing else is in it);
  * DefianceLoader build: spawn_framework.dll and spawn_framework.plugin.json in DefianceLoader\plugins.
  Campaign saves that already contain a UAV Carrier may fail to load without the mod.
# SHOUT_OUTS
  Many thanks to:
- Anthropic Claude, for helping me create this mod
- My family for endless support
  - Gutwormy, the creator of Dark Fate Trainer, for his mod, which inspired me on how to organize the mod files.
  Terminator: Dark Fate Defiance and its DLC belong to Slitherine Ltd.
