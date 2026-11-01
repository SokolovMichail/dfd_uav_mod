# Phase 0: spawn probe

`SpawnProbe.dll` is a throwaway plugin that checks in the real game what the Spawn Framework will rely on. It
changes nothing but two hooks and writes everything it sees to `bin\plugins\SpawnProbe.log`. Addresses below are
RVAs in GOG 1.09.1221 `logic.dll` (`game.dll` where marked); the probe finds them itself (RTTI, byte checks).

## Running it

1. Close the game, then `cmake --build cmake-build-debug --target deploy_probe`. This installs `bin\winhttp.dll`
   (the same loader as DFDRepairMod's), `bin\plugins\SpawnProbe.dll` + `SpawnProbe.ini` and `mods\SpawnProbe`.
2. In MODS enable **Spawn Probe** and disable **HEMTT Spares** (both ship `cars.xml`; only one wins).
3. Skirmish as Resistance or Founders, buy **Probe_uav_van** and **Probe_gun_van** (shown by their sysnames).
   Deploy each (F4), wait a few seconds, then pack it up (F4 again).
4. Send back `bin\plugins\SpawnProbe.log`, and say what you saw: how the drones die on pack-up (fall? explode? any
   corpses left?), whether the gun van's kits go 5 → 4 in its ammo panel and it stops spawning after 5, whether its
   launcher ever fires a kit by itself, and what command points did.

Removal: delete `bin\plugins\SpawnProbe.dll`, `SpawnProbe.ini` and `mods\SpawnProbe`.

`SpawnProbe.ini` sets the vehicles to probe (`filter`, sysname prefix `Probe_`), the squad (`squad`, or
`squad_<vehicle sysname>`), the kit ammo (`uav_kit`), offset (8 m along world X), `max_spawns` (10 per run) and how
the squad dies (`kill = detonate | queue`), read at game start. Deploying replaces it with the project's copy.

## Run 1 (probe v1, 2026-11-01)

- **Works:** spawning (`Lgn_swarm` as team `player` came out as the player's squad, controllable); the F4 start and
  end hooks (three deploy/pack cycles of one van paired up, `flags 1`); positions are (x, y, z), z up.
- The probe fired for every F4 vehicle, the AI's too: an enemy supply truck (team id 1) used up a spawn. The player's
  team id is 0, the enemy's 1 - team ids look like the row order of `forces.xml`.
- **Depot:** 0 entries on the van with `ammunition = heavy_sg:5`; presumably the depot holds only ammo some gun of the
  vehicle fires. The van has only passenger dummy guns.

## Probe v2

- Only vehicles whose sysname starts with `Probe_`; the sysname is `entity->vfunc(+0x48)` (the prototype path,
  `…#Probe_uav_van`) - the way Lua `game.getPrototypeName` (`game.dll` `0x232ec0`) gets it.
- The squad gets the vehicle's own team: its name from the id by the `forces.xml` order; the log says whether the
  spawned squad's team id matches.
- On F4 end the squad is killed: looked up by id (`world->vfunc(+0x10)->vfunc(+0x60)(&ref, id)`), then each soldier
  into the engine's kill queue, as the mission action `killAction` does.
- A second test vehicle, `Probe_gun_van` (a copy of `Int_guntruck`, whose `gl_m6` launcher fires `heavy_sg:16`): does
  its depot list `heavy_sg`, and does taking one show in the ammo panel?

## Run 2 (probe v2, 2026-11-01)

- **Works:** only `Probe_` vehicles fire; the squad's team id matches the van's (team ids = `forces.xml` row order);
  the kill on pack-up (`4 unit(s) queued`).
- **Kill queue = removal, not a combat death:** the drones die but their corpses hang in the air until the queue
  removes them after a delay (`0x875e1c`); the death that makes flyers fall is the damage path.
- **Depot:** the gun van (`Int_guntruck` copy) lists `ammo_50cal` and `heavy_sg` - the depot holds the ammo its guns
  fire. Fields: `+0x28` capacity (16), `+0x2c` rounds (16, what the ammo panel shows), `+0x30` - 1 for both (not the
  rounds: slot 17 `take` works on it, which is why the smoke rounds did not go down). Slot 22 `+0xb0` consume(ammo, n)
  takes from the rounds and calls the depot's change notification (`0x122e40`); slot 20 `+0xa0` adds.

## Probe v3

- **Self-detonation** instead of the kill queue (after the user's hint: bomb drones are in the game). Bomb drones
  are the swarm with the upgrade `Lgn_swarm_explosive` (`explode_damage = ap240 6-8:he200 6-12:fr160`, radius 8,
  `destruction_swarm_explosive`, perk `noCorpse`): their blast is their death. The server side of the self-detonate
  command (`0x1145a0`, sends `net::SelfDetonate`) calls the squad's AiFacet `+0x370(true)` and `0x532f60(squad)`, which
  hits every soldier with a lethal hit (a static damage list `0x87bc00`, radius 10) through the damage call
  `0x52dff0` - they die as if hit, with their row's `explode_damage` and `destroy_effect`. The blast is set in the
  drone's `humans.xml` row.
- **Kits:** the UAV kit is ammunition `uav_kit` (a copy of `heavy_sg`, `cost_recharge = 200`, `ammo_class = none`) in a
  launcher copied from the Stryker's; `Probe_gun_van` carries 5 and spends one per spawn (slot 22); none left - no spawn.
- **One-drone bomb squad** `Probe_bomb_uav` (`Probe_bomb_drone:1`, `noCorpse`, blast `1-3:he60` over 3 m) for the gun
  van; `Probe_uav_van` keeps the scout swarm.

The probe mod now also replaces `ammunition.xml`, `guns.xml`, `gun_mounts.xml`, `humans.xml` and `squads.xml`.

## What the probe tests

| Question | How the probe answers it |
|---|---|
| Is the first tick of `AiDoRechargeState` the moment of F4, and its destructor the end of it (pack, death, orders)? | `F4 start` / `F4 end` lines |
| Does the engine's spawn function work from a plugin, called from that tick? | `spawn: entity … id …`, and drones on screen |
| Team: does the `player` team name give the player's squad; what id do the van and the drone have? | `vehicle … team id` and `spawn: … team id` |
| Can the plugin read and spend the van's ammunition? | `depot` lines: entries, `took 1 'heavy_sg' (5 -> 4)` |
| Is the vehicle's position what we think? | `vehicle at (x, y, z)` |

## Findings so far (disassembly, not yet seen in game)

**Spawning.** Lua `game.spawnSquad` (`game.dll` `0x234380`) calls `LogicUtilsImpl` slot 147 (`+0x498`,
`0x2fde50`), a wrapper around the spawn function `0x526d10`:
`entity* spawn(const float pos[3], world, const std::string& team, const std::string& sysname, float angle,
context, 6 × nullptr, -1)`; the context is a global the wrapper takes with `lea rcx` at `+0x4c`. The result's
`vfunc(+0x40)` is the new unit's id. Nine places in the engine call `0x526d10` (triggers, reinforcements…).
`world` is what `entity->vfunc(+0x78)` returns (the same object the diplomacy check uses).

**Teams.** A team in the spawn call is a name from `forces.xml` (`player`, `player_ally`, `resistance`, …). On an
entity it is an id: `cache = entity->vfunc(+0xb0)`, `[cache+0x20]` is its `TeamFacet` (vtable `0x73fee0`), slot 10
`+0x50` the id (`[+0x118]`), slot 12 `+0x60` (`0x46bd30`) changes it.

**Position.** `[cache+0x00]->vfunc(+0x58)` returns `float[3]` (the engine's own spawn callers use it).

**F4 state.** `AiDoRechargeState` (vtable `0x708240`): `[+0x20]` is the reference block of the supply component it
runs. Slot 6 (`0xbffc0`) runs every tick and switches the component on if it is off; slot 0 (`0xbfc50`, the deleting
destructor) switches it off. Neither is hooked by SparesSupply, so both frameworks can run together.

**Ammunition.** A vehicle's own ammo stock (`cars.xml` `ammunition`, e.g. the Stryker's `heavy_sg:16`) lives in its
`AmmunitionDepotHelper` (vtable `0x7109a0`, built by `0x1212a0`): `[cache+0x28]` (the `AiFacet`) `->vfunc(+0x1c8)`
(`[ai+0x148]`). Slot 9 `+0x48` is the vector of 0x48-byte entries (`+0` ammunition description, `+8` sysname,
`+0x2c` maximum, `+0x30` count); slot 16 `+0x80` count(ammo), 17 `+0x88` take(ammo, n) → taken, 18 `+0x90`
add(ammo, n), 12 `+0x60` missing(ammo). In `ammunition.xml` `cost_recharge` is what a round costs to restock
(`heavy_sg` 8) - a UAV kit is a round with `cost_recharge = 200`.

**Removing a unit.** Lua `game.disappear` (`game.dll` `0x234660`) finds the entity by id and calls its deleting
destructor directly (`[obj]->vfunc(0)(1)`) - probably why it crashes the game on a tractor with a trailer. Not used.
The mission action `killAction` (`KillUnitsAction`, vtable `0x6f8740`, execute `0x339d0`) kills properly: for a squad
(`vfunc(+0x98)(0x10)`) it takes the soldiers (`[cache+0x28]->vfunc(+0x3d0)->vfunc(+0x68)`, a vector of entities) and
queues each with `0x292d70(0x542360(world), unit)` - the world's kill manager and its queue, so the kill is deferred.

**Custom table columns.** The game keeps every column of a table, known or not. MoraleStamina reads its own
`squads.xml` columns from the game's table storage (it hooks the row lookup to get it) - the way to give
`cars.xml` the framework's columns. That hook would clash with MoraleStamina under DefianceLoader, so the storage
must be found another way. Still to do.
