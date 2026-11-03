**Title:** table-merge: tables the game caches before the patch plan commits are never merged (startup race)

## Summary

With `[defiance.table-merge] enabled = true`, a patch that adds rows to several related tables is merged into
some of them and not others, and which ones changes from one start to the next. The game then builds units that
point at rows it cannot find: in our case new vehicles whose new gun mounts are "not found", so the vehicles are
missing from shops and the trainer.

The patches themselves are fine: on another start the same files merge into every table. The cause is timing:
table-merge's hooks go live only when the startup patch plan commits, and by then the game may already have
loaded some tables into its table caches, unmerged. A cached table is never loaded again, so it stays unmerged.

## Versions

- DefianceLoader release 1.9.0 (`DefianceLoader 0.11.0 (ABI 5)`), `defiance.table-merge` 0.1.0
- Game: GOG 1.09.1221 (the loader reports the build as `gog-2026-09-25`; table-merge installs at
  `galileo+0x28fa0, +0x2a6b0`)
- `full_file_diff = false`; two mods enabled, both with patches only (no whole tables)

## How to reproduce

1. A mod with patches that add new rows to `scripts/species/gun_mounts`, `guns` and `cars`, where the new `cars`
   rows name the new mounts in `gun_mounts` (any new row in one table that refers to a new row in another will do).
2. Enable table-merge and the mod; start the game several times.
3. On some starts the game log has `Sysname '<new mount>' in table 'scripts/species/gun_mounts.txt' not found`
   for the new vehicles, and they are missing in game. On other starts everything is there.

## Evidence

The same files, two starts on the same day, ten minutes apart.

**Start A (works).** The hooks go live at "loader ready"; every table is merged after it, in the order the game
loads them:

```
19:37:09.621 [defiance.table-merge] table merge installed (galileo+0x28fa0, +0x2a6b0): 2 enabled mods ...
19:37:10.352 loader ready
19:37:10.487 table merge: scripts/species/squads: patchwork_uav updated 0 rows, added 2, deleted 0
19:37:10.522 table merge: scripts/species/gun_mounts: patchwork_uav updated 0 rows, added 2, deleted 0
19:37:10.543 table merge: scripts/species/guns: patchwork_uav updated 0 rows, added 1, deleted 0
19:37:10.725 table merge: scripts/species/ammunition: patchwork_uav updated 0 rows, added 1, deleted 0
19:37:10.817 table merge: scripts/species/cars: patchwork_uav updated 0 rows, added 3, deleted 0, new columns ...
```

**Start B (fails).** Plugin init took longer; "loader ready" came 3.4 s after the first plugin started, and the
game started building its unit data just before it:

```
19:47:25.896 [defiance.table-merge] table merge installed (galileo+0x28fa0, +0x2a6b0): 2 enabled mods ...
19:47:26.535 (game.log) [CinematicGameState.cpp:52] Create CinematicGameState
19:47:26.567 loader ready
19:47:26.709 table merge: scripts/species/ammunition: patchwork_uav updated 0 rows, added 1, deleted 0
19:47:26.767 table merge: scripts/species/cars: patchwork_uav updated 0 rows, added 3, deleted 0, new columns ...
19:47:26.879 (game.log) [ScriptInfosCache.cpp:6790] Sysname 'uav_kit_mount' in table 'scripts/species/gun_mounts.txt' not found
19:47:26.883 (game.log) [ScriptInfosCache.cpp:8574] Shoot point 'korpus.bashnya.turret01.sight01' in 'Int_recon_van' not found.
19:47:26.883 (game.log) [ScriptInfosCache.cpp:6790] Sysname 'uav_kit_mount_van' in table 'scripts/species/gun_mounts.txt' not found
19:47:34.369 table merge: scripts/species/gun_mounts: patchwork_uav updated 0 rows, added 2, deleted 0
19:47:34.375 table merge: scripts/species/guns: patchwork_uav updated 0 rows, added 1, deleted 0
19:47:34.722 table merge: scripts/species/squads: patchwork_uav updated 0 rows, added 2, deleted 0
19:47:41.843 table merge: scripts/species/humans: patchwork_uav updated 0 rows, added 2, deleted 0
```

The game read `cars` merged (26.767) and looked its new rows' mounts up in a `gun_mounts` that had no merge
(26.879). `gun_mounts`, `guns` and `squads` were merged only 8 s later, on a later load; by then the units were
built. In both starts `humans` is merged late (26.2 s and 41.8 s), so it may be read unmerged every time.

## Cause

- table-merge merges inside the table providers' dispatchers (`plugins/table-merge/src/sites.rs`,
  `native.rs` `provide`), which run only when a table is **not** in the provider's cache yet.
- Its hooks are managed: `hook_exact` "Managed requests are staged until the runtime patch plan commits"
  (`crates/api/src/lib.rs`), and the host commits once every plugin's `init` has run (`crates/loader/src/host.rs`,
  `finalize_patch_plan` → `hooks::commit_staged`).
- The host runs on its own thread while the game keeps running. Whatever the game loads into the ANSI or UTF-8
  table cache before the commit is cached unmerged, and later lookups of that table hit the cache, not the hook.

So the result depends on how long all plugins' `init` takes compared with when the game starts reading data
tables. Above, 3.4 s of plugin init (23.2 s → 26.57 s) against the game's unit data at 26.5 s.

## Proposed fixes

### 1. Merge the tables already cached when the hooks go live (table-merge only)

On the first call of either dispatcher detour (that is, the first moment the hooks are live), walk both
providers' caches once and merge every cached table that has patches (or full copies) into it in place, with
the existing `merge_into`; afterwards everything works as now.

What we found in galileo.dll (GOG 2026-09-25 RVAs): galileo has no RTTI. The ANSI and UTF-8 providers are built
together in one constructor (`0x20550`), at `+0x948` and `+0x968` of one object. Each is its vtable
(`0x331e0` / `0x33270`, ten slots; the hooked dispatcher is the last, `+0x48`) followed by a `std::map` (the
constructor allocates its 0x48-byte head node and sets `_Color/_Isnil = 0x0101`), keyed by the table name - the
cache. The node's mapped value looks like an 8-byte pointer to the table (not verified further).

- Pros: inside the experimental plugin only; nothing changes for other plugins or for startup.
- Cons: needs a pattern for the object holding the providers on every supported build, and a read-only walk of
  MSVC map nodes. A table the game already *consumed* before the commit stays missed (a much smaller window).
  Thread safety: in start B the first hooked load (26.709) and the unmerged lookup (26.879) look like the same
  thread building unit data, so doing the walk synchronously in that first call should pause exactly that
  thread; whether galileo locks its cache, and so whether another thread could read a table while it is
  rewritten, is not checked.

### 2. Hold the game until the patch plan is committed (loader)

The game calls `CreateDXGIFactory*` early, when it creates its graphics device (23.46 s in start B, after
`logic.dll`/`game.dll` are loaded and well before it loads data tables). The proxy's forwarder for those exports
could wait, on its first call, until `DEFIANCE_LOADER_STATE` is READY or FAILED (with a timeout), then continue
into the real DXGI. Behind a `[loader]` setting.

- Pros: closes the race for every plugin whose hooks must be live before the game reads data, not just
  table-merge.
- Cons: plugin init time is added to every start, with the window possibly not responding meanwhile; a plugin
  whose `init` waits on something the game's main thread does would deadlock until the timeout; the EXE-launcher
  start path has no such call and needs its own answer. A change to the loader's startup design.

We are happy to send a PR for either, or to test a build on our setup, where start B happens often.
