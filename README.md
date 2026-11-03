# Spawn Framework + UAV Carriers

A mod for Terminator: Dark Fate - Defiance (GOG 1.09.1221). A vehicle deployed in F4 mode keeps a squad of its own
next to it - the **Spawn Framework** (a plugin and a small game mod, shared by every mod made on it), and
**UAV Carriers**, a mod made on it: two carriers that keep a recon drone in the air while deployed.

- Integrators' **Recon UAV Carrier** (on their gun truck) - a clean drone, a little slower than the Legion's swarm scouts.
- Resistance's **Patchwork UAV Carrier** (on their van) - one working drone put together from ten wrecked Legion
  scouts: slow, rusty, its eye glowing green.

Each carrier carries 5 UAV kits (500 to restock each) and spends one per drone, and no passengers. One drone per
carrier at a time; it dies when the carrier packs up or is destroyed, and the next one waits 60 s. They are in every
shop of the main campaign and the Canada DLC, the first zone of the Resistance DLC's uprising, and the Resistance and
Founders skirmish armies.

The layout, build and tools follow [DFDRepairMod](../DFDRepairMod) (HEMTT Spares): the same `winhttp.dll` plugin
loader, `common/patch.h` and `common/guard.h`, `re/` and `mod/tools/`.

| | |
|---|---|
| [docs/MODDING.md](docs/MODDING.md) | making vehicles on the framework |
| [docs/Probe.md](docs/Probe.md) | how the engine was probed: addresses, what was checked in game |

## Layout

| Folder | |
|---|---|
| `plugins/spawn/core/` | the rules (tethered and untethered vehicles, kits, cooldowns, saved games), reading the framework's table columns, which tables are read (`mods.h`), F4's look (`look.h`) - no Windows, no game memory |
| `plugins/spawn/logic_dll/` | the binding to today's `logic.dll`: the game's objects (`engine.h`), the rules' port (`port.h`), the hooks (`hooks.cpp`), the tables; `standalone.cpp` / `defiance.cpp` - the two builds |
| `plugins/spawn/game_dll/` | the binding to `game.dll`: F4's cooldown, each vehicle's own F4 pictures and tooltips (`panel.cpp`) |
| `mod/framework/` | the Spawn Framework mod (`build_framework_mod.py`), what mods made on it share (`framework.py`), the F4 picture tool (`art/buttons.py`) |
| `mod/uav_carriers/` | UAV Carriers (`build_mod.py`) and its drone icon (`art/front_drone.py`) |
| `mod/tools/` | the table and archive tools both builds use |
| `plugins/spawn_probe/`, `mod/probe/` | the Phase 0 probe (kept for reference) |
| `tests/` | `core_test` - the core through a test port |
| `re/` | the disassembly tools the engine was studied with ([re/README.md](re/README.md)) |
| `tools/` | `gen_winhttp_proxy.py` - the standalone loader's export forwarding |

## Build

CLion's bundled MinGW and CMake; `.env` with `DFD_PAK_PASSWORD` (see `.env.example`) for the game archives.

```bash
cmake -S . -B cmake-build-debug -G Ninja
```

```bash
cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug
```

```bash
cmake --build cmake-build-debug --target deploy
```

`deploy` (the game must be closed) installs `bin\plugins\SpawnFramework.dll`, `mods\Spawn_Framework` and
`mods\UAV_Carriers`, and removes the probe. It leaves `bin\winhttp.dll` as it is: that is already a loader of
`bin\plugins\*.dll` (ours from DFDRepairMod, or the trainer's). On a game without one, `deploy_with_loader` installs
ours too. `deploy_defiance` installs the DefianceLoader build instead (`DefianceLoader\plugins\spawn_framework.dll`
and its manifest) and removes the standalone `SpawnFramework.dll`. Then enable **Spawn Framework** and
**UAV Carriers** in MODS; disable other mods that ship `cars.xml` (HEMTT Spares).

Development builds add the framework's test vehicles to UAV Carriers (the CMake option `SPAWN_TEST_VEHICLES`, on
by default; off for a release): the **Test Troop Van (untethered)** in the Resistance army - two marksman squads per
launch, the framework's default F4 pictures.

## Roadmap

Done on branches, not in `dev` yet:

- **Recall** (`feature/recall`) - packed up with its whole squad inside the vehicle's circle, the vehicle takes the
  squad back without a death and gets its kit back if it has room. Needs the pack-up picture and tooltip to match.
- **Pack-up picture** (`feature/pack-icon`) - the carriers' drone struck out; the framework's default is the game's
  own pack-up button; "Mobile mode" tooltips.
- **Release 1.0** (`feature/release-1.0`) - the version, the MODS menu icons, UAV Carriers' cover and banner. Still
  to do: the release archives and their readme, a build with the test vehicles off.
- **DefianceLoader patch build** (`feature/defiance-patches`) - table-merge patches instead of whole tables, so the
  mod no longer clashes with HEMTT Spares. Waits for DefianceLoader to fix its table merge (it misses the unit
  cache's first reads).

Ideas:

- **Spawning vehicles and helicopters.** Today the framework spawns squads only (`squads.xml`: infantry, drones).
  `game.dll` has Lua `spawnCar`, `spawnTank` and `spawnHeli` next to `spawnSquad`, so the engine can make vehicles.
  Needs: the logic.dll function behind them (a probe, as for squads); a crew - a squad that can drive it
  (`squads.xml` `driving_classes`); "alive" and "destroy" for a vehicle in `SpawnPort` (today: soldiers alive, and
  their self-detonation); a column saying what to spawn.
- **The carriers' own circle** instead of the ammunition supply circle they show now (it clashes with HEMTT Spares,
  which changes the same circle code).
- **Enemy AI using carriers.**
- **A tethered vehicle with several squads** (`spawn_count` above 1; a saved game keeps one squad's link today).
- **The Patchwork UAV smoking** (`spawnSmoke`, as `Lgn_swarm_smoke`), and a small chance that a reflashed scout goes
  back to Legion factory settings.
