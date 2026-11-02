# Making vehicles for the Spawn Framework

The Spawn Framework lets a vehicle of Terminator: Dark Fate - Defiance keep a squad of its own next to it while it
is deployed (F4). Your mod only describes the vehicle in the game's tables; no code is needed.

- Deployed, the vehicle spawns its squad at once, if it has a kit left.
- One squad per vehicle at a time.
- The squad dies when the vehicle packs up, is destroyed, or leaves F4 mode for any other reason - also inside a
  building or a vehicle (there a few seconds later: the self-detonation does not reach it, the engine's kill queue
  does).
- A squad lost in battle packs its vehicle up by itself.
- After the squad dies (in battle or with its vehicle's pack-up), the vehicle waits a cooldown before the next one.
  F4 shows the wait: greyed out, filling up, and it cannot be pressed until the wait is over.
- Each squad costs one round of the vehicle's own ammunition - the "kit". No kit left, no squad.

That is a **tethered** vehicle, the default. An **untethered** one (`spawn_tethered` false) only launches:

- Deployed with its cooldown over, it launches `spawn_count` squads, a kit each (as many as it has kits for), and the
  cooldown starts at once.
- The squads are on their own from then on: packing up, the vehicle's destruction or the squads' deaths change
  nothing.
- One launch per deploy: the next needs a pack-up and a deploy after the cooldown.

On the framework's vehicles F4 shows the vehicle's own pictures and tooltips (see "F4's look"); other vehicles' F4
stays as the game draws it.

Patchwork UAV, the framework's example mod, adds two vans that keep a recon drone in the air.

## What the framework is

| Part | Where | What it does |
|---|---|---|
| Plugin | `bin\winhttp.dll` + `bin\plugins\SpawnFramework.dll`, or the DefianceLoader plugin `DefianceLoader\plugins\spawn_framework.dll` | the mechanics, in the game's `logic.dll` and `game.dll` |
| Spawn Framework mod | `mods\Spawn_Framework` | F4's layout (`scripts/ui/AbilityButtons/pack.txt` with the framework's two progress bars) and the default F4 pictures |

Players install the framework once - the plugin and its mod - then your mod, and enable both mods in the game's MODS
menu. Your mod ships no `pack.txt`: every mod made on the framework shares the framework mod's.

## Make a vehicle

A row of `scripts/species/cars.xml` or `tanks.xml`, with the framework's columns added to the table (the game ignores
columns it does not know):

| Column | Value | |
|---|---|---|
| `spawn_squad` | a `squads.xml` sysname | the squad the vehicle keeps; required |
| `spawn_ammo` | an `ammunition.xml` sysname | the kit: one round is spent per squad; empty - squads cost nothing |
| `spawn_cooldown` | seconds | game time after the squad dies (untethered: after the launch) before the next one; default 60 |
| `spawn_tethered` | `true` / `false` (`0`, `no`) | `true` (default) - the squad belongs to the vehicle; `false` - the vehicle only launches |
| `spawn_count` | 1 to 20 | squads per launch, untethered vehicles only for now (a tethered one keeps one squad: a saved game keeps the link to one); default 1 |
| `spawn_icon`, `spawn_icon_cooldown`, `spawn_icon_recall` | UI pictures, e.g. `main\buttons2\pwuav_ready.dds` | F4's look, see below; empty - the framework's defaults |
| `spawn_tooltip`, `spawn_tooltip_recall` | `locale/strres.xml` keys | F4's tooltips, see below; empty - the framework's defaults |

And in the usual columns:

| Cell | Value | Why |
|---|---|---|
| `abilities` | `ammunition_recharge` | the F4 button: the game gives it only to supply vehicles. Nothing else may be next to it (`refuel`, `support`, `repair` take it away) |
| `capacity_ammo_recharger` | `0` | the vehicle hands out no ammunition in F4 mode |
| `ammunition` | e.g. `uav_kit:5` | the kits the vehicle carries, with its other ammunition |
| `gun_mounts` | one mount whose gun loads the kit | **a vehicle stores only the ammunition one of its guns fires** - without such a gun the kits are not there |

The kit is an ammunition row of your own, so the player sees it in the vehicle's ammunition panel and supply
vehicles restock it (`cost_recharge` - what one kit costs). Patchwork UAV's `uav_kit` is a copy of the smoke round
`heavy_sg` with `cost_recharge = 200` and `ammo_class = none` (nothing is a target for it, so its launcher never
fires); the launcher (`guns.xml` `uav_kit_launcher`, a copy of `gl_m6` with `ammunition = uav_kit:1`) sits in a
mount of the vehicle (`gun_mounts.xml`, a copy of one of the vehicle's own mounts with `gun = uav_kit_launcher`).

### F4's look

The framework mod's `pack.txt` adds two progress bars over F4's buttons; on a framework vehicle the plugin shows
them with the vehicle's own pictures (any other vehicle's F4 is left alone):

| Column | The picture | Default |
|---|---|---|
| `spawn_icon` | the deploy button, ready - and what the cooldown fills over | the generic rifle squad's icon |
| `spawn_icon_cooldown` | what the cooldown fills it with: the part of the wait still to go | a dim grey copy of it |
| `spawn_icon_recall` | the pack-up button | it with an arrow down |

Pictures are named the way layouts name them: `main\buttons2\x.dds` is the file
`basis/textures/ui/pictures/Main/buttons2/x.dds` of your mod. Give them a prefix of your own, so no other mod's
file is in the way. `mod/framework/art/buttons.py` makes the three from any icon, the way the game draws its own
timer buttons: `python mod/framework/art/buttons.py <the game's buttons_unpack_n.dds> <icon.dds> <prefix>`.

The tooltips: `spawn_tooltip` (deploy) and `spawn_tooltip_recall` (pack-up) are `locale/strres.xml` keys, each with
`<key>_desc` and `<key>_add` rows, `%s` - the hotkey - in the first; at most 15 characters (the plugin writes the key
into the game's own string in place; a longer one shows the default, and the log says so). Your mod's
`localization/en/basis_en/locale/strres.xml` carries your rows and the framework's defaults (`spawn_deploy`,
`spawn_recall` - `mod/framework/framework.py` has them): a whole `strres.xml` replaces another, so every mod made on
the framework ships the defaults too. Two such mods' `strres.xml` still replace each other; under DefianceLoader's
table merge each mod can ship only its own rows (to do).

### The squad

Any squad works. How it dies when its vehicle packs up is the engine's own self-detonation of bomb drones: every
soldier takes a lethal hit and dies as if hit - so its `humans.xml` row decides what that looks like
(`explode_damage`, `explode_splash_radius`, `destroy_effect`; the bomb drones' is `ap240 6-8:he200 6-12:fr160` over
8 m with `destruction_swarm_explosive`). The perk `noCorpse` in `squads.xml` leaves no body. Mind the blast: it
also hits whatever stands next to the squad when its vehicle packs up.

The squad is of its vehicle's team and appears 8 m from it.

## Things to know

- **The game replaces whole files.** A mod that ships `cars.xml` replaces the game's and every other mod's; two
  vehicle mods that both ship it conflict. Ship only the tables you change.
- **Where the framework reads the columns.** From the files of the mods enabled in MODS
  (`Documents\My Games\Terminator Dark Fate\mods\mods.json`): every `cars.xml`, `tanks.xml`, `cars.<x>.patch.xml`,
  `tanks.<x>.patch.xml` in the mod folder. A vehicle in several mods takes the last mod's row.
- **Check it works.** The framework's log - `bin\plugins\SpawnFramework.log`, or the DefianceLoader log for
  `sokolov.spawn-framework`: at the first F4 of any vehicle it lists the spawning vehicles it found
  (`Res_patchwork_van -> Res_patchwork_uav (kit uav_kit, cooldown 60 s)`), then the first deploys, spawns and kills.
- **Single player only.** Not tested in multiplayer.
- **Saved games.** A vehicle keeps its squad and its cooldown through a save and a load: the framework keeps them in
  the vehicle's F4 button, which the game saves (its timer - the cooldown left, its length - the squad out). A save
  made with the framework should load without it too (nothing else reads that length; not tested yet).