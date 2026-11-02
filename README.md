# Spawn Framework + Patchwork UAV

A mod for Terminator: Dark Fate - Defiance (GOG 1.09.1221). A vehicle deployed in F4 mode keeps a squad of its own
next to it - the **Spawn Framework** (a plugin and a small game mod, shared by every mod made on it), and
**Patchwork UAV**, a mod made on it: two vans that keep a recon drone in the air while deployed.

- Integrators' **Recon Van** (on their gun truck) - a clean drone, a little slower than the Legion's swarm scouts.
- Resistance's **Patchwork UAV Van** - one working drone put together from ten wrecked Legion scouts. Slow.

Each van carries 5 UAV kits (200 to restock each) and spends one per drone. One drone per van at a time; it dies
when the van packs up or is destroyed, and the next one waits 60 s.

The layout, build and tools follow [DFDRepairMod](../DFDRepairMod) (HEMTT Spares): the same `winhttp.dll` plugin
loader, `common/patch.h` and `common/guard.h`, `re/` and `mod/tools/`.

| | |
|---|---|
| [docs/MODDING.md](docs/MODDING.md) | making vehicles on the framework |
| [docs/Probe.md](docs/Probe.md) | how the engine was probed: addresses, what was checked in game |

## Layout

| Folder | |
|---|---|
| `plugins/spawn/core/` | the rules (one squad per vehicle, kits, cooldown) and reading the framework's table columns - no Windows, no game memory |
| `plugins/spawn/logic_dll/` | the binding to today's `logic.dll`: hooks, engine calls, finding and patching; `standalone.cpp` / `defiance.cpp` - the two builds |
| `mod/framework/` | the Spawn Framework mod (`build_framework_mod.py`), what mods made on it share (`framework.py`), the F4 picture tool (`art/buttons.py`) |
| `mod/patchwork/` | Patchwork UAV (`build_mod.py`) and its drone icon (`art/front_drone.py`) |
| `mod/tools/` | the table and archive tools both builds use |
| `plugins/spawn_probe/`, `mod/probe/` | the Phase 0 probe (kept for reference) |
| `tests/` | `core_test` - the rules through a test port |

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

`deploy` (the game must be closed) installs `bin\plugins\SpawnFramework.dll`, `mods\Spawn_Framework` and `mods\Patchwork_UAV`, and removes
the probe. It leaves `bin\winhttp.dll` as it is: that is already a loader of `bin\plugins\*.dll` (ours from
DFDRepairMod, or the trainer's). On a game without one, `deploy_with_loader` installs ours too. Then enable
**Spawn Framework** and **Patchwork UAV** in MODS; disable other mods that ship `cars.xml` (HEMTT Spares). The vans are in the Resistance and
Founders skirmish armies.
