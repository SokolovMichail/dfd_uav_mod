# Making vehicles for the Spawn Framework

The Spawn Framework lets a vehicle of Terminator: Dark Fate - Defiance send out units of its own when it is deployed
(F4). Any vehicle can use it; your mod only describes the vehicle in the game's tables. No code is needed.

- **Tethered vehicle** (the default). Deployed, it keeps one squad of its own next to it - a drone, or any squad.
  The squad dies when the vehicle packs up, is destroyed or leaves F4 mode for any other reason; a squad lost in
  battle packs the vehicle up by itself. After the squad dies, the vehicle waits a cooldown before the next one.
- **Untethered vehicle.** Deployed, it launches a set number of squads, and packs up again by itself: F4 is its
  launch button. The squads are on their own from then on; the cooldown starts at the launch.
- **Kits.** Each squad costs one round of the vehicle's own ammunition - the "kit". No kit left, no squad.
- **F4 shows the wait.** While the cooldown runs, F4 is greyed out and fills up, and cannot be pressed. On the
  framework's vehicles F4 shows each vehicle's own pictures and tooltips; other vehicles' F4 stays as the game draws
  it.
- **Saved games.** A vehicle keeps its squad and its cooldown through a save and a load.

UAV Carriers, the framework's example mod, adds two tethered vehicles: UAV carriers that keep a recon drone in the
air.

## What the framework is

| Part | Where | What it does |
|---|---|---|
| Plugin | `bin\winhttp.dll` + `bin\plugins\SpawnFramework.dll`, or the DefianceLoader plugin `DefianceLoader\plugins\spawn_framework.dll` | the mechanics, in the game's `logic.dll` (squads, kits, cooldowns) and `game.dll` (the F4 button) |
| Mini-mod | `mods\Spawn_Framework` | F4's layout (`scripts/ui/AbilityButtons/pack.txt` with the framework's two progress bars) and the default F4 pictures |

Players install the framework once, then your mod. In the game's MODS menu they enable both "Spawn Framework" and
your mod.

## Make a vehicle

Add a row to `scripts/species/cars.xml` or `scripts/species/tanks.xml`. Other tables (helicopters, for example) are
not supported. The easiest start is to copy a vehicle's row, add the framework's columns to the table (the game
ignores columns it does not know) and change these cells.

### A tethered vehicle

The framework's columns:

| Column | Value | Why |
|---|---|---|
| `spawn_squad` | a `squads.xml` sysname | the squad the vehicle keeps; required - a row without it is not the framework's |
| `spawn_ammo` | an `ammunition.xml` sysname | the kit: one round is spent per squad; empty - squads cost nothing |
| `spawn_cooldown` | seconds | game time after the squad dies before the next one; default 60 |
| `spawn_tethered` | `true`, or empty | the default |

And in the game's columns:

| Cell | Value | Why |
|---|---|---|
| `abilities` | `ammunition_recharge` | the F4 button, see below |
| `capacity_ammo_recharger` | `0` | the vehicle hands out no ammunition in F4 mode |
| `ammunition` | e.g. `uav_kit:5` | the kits the vehicle carries, with its other ammunition |
| `gun_mounts` | one mount whose gun loads the kit | **a vehicle stores only the ammunition one of its guns fires** - without such a gun the kits are not there (see "The kit") |

**Why `ammunition_recharge`.** The game gives the F4 button only to supply vehicles: ones with ammunition or fuel
supply. The framework hangs the deploy on the ammunition component. So every framework vehicle needs
`ammunition_recharge`, even though it hands out no ammunition: give it `capacity_ammo_recharger=0`, and F4 still
works. Nothing else may be next to it: `refuel`, `support` and `repair` take it away. Deployed, the vehicle shows the
ammunition supply circle that comes with it; the framework does not change it yet (an own circle would clash with
HEMTT Spares', which changes the same circle code).

Without the framework such a vehicle is an ordinary one with a launcher that never fires and the F4 button of a
supply vehicle with nothing to hand out.

### An untethered vehicle

The same as a tethered one, with:

| Column | Value | Why |
|---|---|---|
| `spawn_tethered` | `false` (or `0`, `no`) | the vehicle only launches |
| `spawn_count` | 1 to 20 | squads per launch, a kit each; default 1 |
| `spawn_cooldown` | seconds | game time after the launch before the next one; default 60 |

The untethered vehicle:
- launches when deployed with its cooldown over, as many squads as it has kits for (none - it launches nothing,
  and no cooldown starts);
- packs up again by itself after the launch;
- forgets its squads: packing up, its destruction or the squads' deaths change nothing.

`spawn_count` works for untethered vehicles only for now: a tethered vehicle keeps one squad (a saved game keeps
the link to one), and the log says when its count is ignored.

### The kit

The kit is an ammunition row of your own, so the player sees it in the vehicle's ammunition panel and supply
vehicles restock it (`cost_recharge` - what one kit costs). Give it its own `image` (and `image_grayed`,
`image_denied`) and a `locale/ammunition.xml` row - name and description - so it does not pass for another round.

UAV Carriers' `uav_kit` is a copy of the smoke round `heavy_sg` with `cost_recharge = 500` and `ammo_class = none`
(nothing is a target for it, so its launcher never fires). The launcher (`guns.xml` `uav_kit_launcher`, a copy of
`gl_m6` with `ammunition = uav_kit:1`) sits in a mount of the vehicle (`gun_mounts.xml`, a copy of one of the
vehicle's own mounts with `gun = uav_kit_launcher`).

### The squad

Any squad works. The squad is of its vehicle's team and appears 8 m from it.

How a tethered squad dies when its vehicle packs up is the engine's own self-detonation of bomb drones: every
soldier takes a lethal hit and dies as if hit - so its `humans.xml` row decides what that looks like
(`explode_damage`, `explode_splash_radius`, `destroy_effect`; the bomb drones' is `ap240 6-8:he200 6-12:fr160` over
8 m with `destruction_swarm_explosive`). The perk `noCorpse` in `squads.xml` leaves no body. Mind the blast: it
also hits whatever stands next to the squad. A squad inside a building or a vehicle takes no such hit: it dies a
few seconds later instead.

### F4's look

The mini-mod's `pack.txt` adds two progress bars over F4's buttons; on a framework vehicle the plugin shows them
with the vehicle's own pictures and tooltips:

| Column | What | Default |
|---|---|---|
| `spawn_icon` | the deploy button, ready - and what the cooldown fills over | the generic rifle squad's icon |
| `spawn_icon_cooldown` | what the cooldown fills it with: the part of the wait still to go | a dim grey copy of it |
| `spawn_icon_recall` | the pack-up button | it struck out with a red slash |
| `spawn_tooltip` | the deploy button's tooltip: a `locale/strres.xml` key | `spawn_deploy` |
| `spawn_tooltip_recall` | the pack-up button's | `spawn_recall` |

- **Pictures** are named the way layouts name them: `main\buttons2\x.dds` is the file
  `basis/textures/ui/pictures/Main/buttons2/x.dds` of your mod. Give them a prefix of your own, so no other mod's
  file is in the way. In the framework's source, `mod/framework/art/buttons.py` makes the three from any icon, the
  way the game draws its own timer buttons.
- **Tooltip keys** have `<key>_desc` and `<key>_add` rows too, and `%s` - the hotkey - in the first; at most 15
  characters (the plugin writes the key into the game's own string in place; a longer one shows the default, and
  the log says so).
- **The default tooltips** come with your mod: a whole `strres.xml` replaces another, so your mod's
  `localization/en/basis_en/locale/strres.xml` carries your rows and the framework's `spawn_deploy` and
  `spawn_recall` ones.

### Rules for both

- **One squad per tethered vehicle at a time**; one launch per deploy for an untethered one.
- **The cooldown runs in game time**, deployed or not, and survives a save.
- **Enemy vehicles** follow the same rules, but the AI has not been tested with them.

## Things to know

- **The game replaces whole files.** A mod that ships `scripts/species/cars.xml` replaces the game's `cars.xml` and
  every other mod's. Two vehicle mods that both ship it conflict, UAV Carriers included: in MODS only the higher one
  wins. Ship only the tables you change. DefianceLoader has an experimental "mod table merge" feature that patches
  tables row by row instead; in DefianceLoader 1.9.0 it does not yet reach every read of the tables a new vehicle
  needs, so the framework's mods do not use it yet.
- **Do not ship the framework's files.** Do not include its DLLs or `mods\Spawn_Framework` in your mod (nor its
  `pack.txt`); ask players to install the framework. Two copies of the plugin do not work together.
- **Where the framework reads the columns.** From the mods enabled in MODS
  (`Documents\My Games\Terminator Dark Fate\mods\mods.json`): every `cars.xml` and `tanks.xml` in the mod's folder,
  the DLCs' (`dlc\<DLC>\basis\...`) included, and the table merge's `cars.<x>.patch.xml` and `tanks.<x>.patch.xml`.
  A vehicle in several mods takes the last mod's row.
- **Your vehicle also needs:**
  - a shop or army entry, so the player can get it;
  - a localized name;
  - the rows in each DLC's own tables, if it should be there: a DLC has its own `cars.xml`, `humans.xml`,
    `squads.xml` and shops;
  - optionally, an encyclopedia article and icons.

  UAV Carriers does all of these.
- **Check it works.** Look in the framework's log: `bin\plugins\SpawnFramework.log`, or the DefianceLoader log for
  `sokolov.spawn-framework`. At the first F4 of any vehicle it lists the vehicles it found:
  - `Res_patchwork_uav_carrier -> Res_patchwork_uav, tethered (kit uav_kit, cooldown 60 s)`;

  then the first events, with the vehicle's address:
  - `deployed` — a framework vehicle went into F4;
  - `squad spawned`, `squad launched (untethered)` — what came out;
  - `packed up: squad killed`, `squad lost: packing up` — how a tethered squad ended;
  - `squad taken back from the saved game` — after a load.
- **Single player only.** Not tested in multiplayer.
- **Saved games.** The framework keeps a vehicle's state in its F4 button, which the game saves: the timer holds the
  cooldown left, the length - while a squad is out - the squad's id. A save made with the framework should load
  without it too (nothing else reads that length; not tested yet).

## The example: UAV Carriers

UAV Carriers clones two vehicles. The clones differ from the originals in these cells:

| Vehicle | Copy of | `gun_mounts` | `ammunition` | `people_capacity` |
|---|---|---|---|---|
| Recon UAV Carrier (`Int_uav_carrier`) | `Int_guntruck` | `stryker_machine_gun,uav_kit_mount` | `ammo_50cal:2000,uav_kit:5` | `0` |
| Patchwork UAV Carrier (`Res_patchwork_uav_carrier`) | `Res_van` | `uav_kit_mount_van` | `uav_kit:5` | `0` |

Both have `abilities=ammunition_recharge` and `capacity_ammo_recharger=0`, and in the framework's columns:

| Column | Recon UAV Carrier | Patchwork UAV Carrier |
|---|---|---|
| `spawn_squad` | `Int_recon_uav` | `Res_patchwork_uav` |
| `spawn_ammo` | `uav_kit` | `uav_kit` |
| `spawn_cooldown` | `60` | `60` |
| `spawn_icon`, `_cooldown`, `_recall` | `main\buttons2\uavc_ready.dds`, `uavc_cooldown.dds`, `uavc_recall.dds` | the same |
| `spawn_tooltip`, `_recall` | `uavc_deploy`, `uavc_recall` | the same |

Each squad is one drone, a copy of the Legion's swarm scout (`Lgn_swarm`) with the perk `noCorpse`, a small blast
when it dies (`explode_damage = 1-2:he30` over 2 m, `destroy_effect = destruction_swarm_explosive`) and its own
speed: 9 for the Recon UAV Carrier's, 6 for the Patchwork one's, which also has a rusty model of its own
(`mesh = uavc_drone`).

Plus new names, the kit and its launcher, encyclopedia articles for both carriers and both drones (the drones' as
squads of their carrier's faction, with a one-drone picture, `tech_pic\infantry_icons_big\uavc_drone.dds`), and shop
entries: one of each carrier in every shop of the main campaign and the Canada DLC, the first zone of the Resistance DLC's uprising, and the Resistance and Founders skirmish
armies. The rows go into the base game's tables and both DLCs'. Open `mods\UAV_Carriers` to see every file.
