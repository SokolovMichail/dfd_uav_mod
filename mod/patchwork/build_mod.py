"""Builds Patchwork UAV (mods\\Patchwork_UAV), a mod made on the Spawn Framework, from the installed game's current
files:

    python mod/patchwork/build_mod.py --game <game folder> --out <mods/Patchwork_UAV> [--cache <dir>] [--version 0.1]

Two vans that, deployed (F4), keep one recon drone of their own in the air. Each carries 5 UAV kits (an ammunition
of its own, 200 to restock each) and spends one per drone; the drone dies when its van packs up or is destroyed, and
the next one waits 60 s after it.

- Integrators: Int_recon_van, on the Integrators' gun truck (Int_guntruck): the smoke launcher is a UAV kit
  launcher. Its drone, Int_recon_uav, is a swarm scout a little slower than the Legion's.
- Resistance / Founders: Res_patchwork_van, the "rusty" one, on the Resistance van (Res_van): one of its six
  passenger mounts holds the kit launcher. Its drone, Res_patchwork_uav, is put together from scrap: slow.

What makes them the framework's is their cars.xml rows (the framework's columns, framework/framework.py); F4's
layout comes with the Spawn Framework mod. Their F4 pictures and tooltips are the mod's own: the front drone of the
swarm drones' icon (art/front_drone.py, made into F4's pictures by the framework's art/buttons.py) and "Deploy
drone" / "Recall drone".

The game files are taken fresh every time (gamefiles.py: the last archive or folder of the layer that has the
file), so after a game update a rebuild is enough. The base layer only for now (skirmish, main campaign).

Without the Spawn Framework (plugin and mod) the vans are ordinary vans with a launcher that never fires and the F4
button of a supply vehicle with nothing to hand out.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.join(HERE, '..')
TOOLS = os.path.join(MOD, 'tools')
for p in (TOOLS, os.path.join(MOD, 'framework'), os.path.join(MOD, 'framework', 'art'), os.path.join(HERE, 'art')):
    sys.path.insert(0, p)
from gamefiles import Game  # noqa: E402
import buttons  # noqa: E402
import framework as fw  # noqa: E402
from front_drone import front_drone  # noqa: E402

KIT = 'uav_kit'
KITS = 5
# The F4 button (a supply component that hands out nothing: the framework hangs the deploy on it).
F4 = ['abilities=ammunition_recharge', 'capacity_ammo_recharger=0']
# A recon drone: a swarm scout alone, no corpse, a small blast when it dies (its van packs up) - the bomb drones'
# death effect, a tenth of their blast.
DRONE = ['explode_damage=1-2:he30', 'explode_splash_radius=2', 'destroy_effect=destruction_swarm_explosive']

UNITS = [
    dict(van='Int_recon_van', van_from='Int_guntruck', van_name='Recon Van',
         van_stats=F4 + ['gun_mounts=stryker_machine_gun,uav_kit_mount', f'ammunition=ammo_50cal:2000,{KIT}:{KITS}'],
         squad='Int_recon_uav', squad_name='Recon UAV', drone='Int_recon_drone',
         drone_stats=DRONE + ['max_velocity=9', 'max_walk_velocity=9']),
    dict(van='Res_patchwork_van', van_from='Res_van', van_name='Patchwork UAV Van',
         van_stats=F4 + ['gun_mounts=res_van_passenger_gun1,res_van_passenger_gun2,res_van_passenger_gun3,'
                         'res_van_passenger_gun4,res_van_passenger_gun5,uav_kit_mount_van',
                         f'ammunition={KIT}:{KITS}'],
         squad='Res_patchwork_uav', squad_name='Patchwork UAV', drone='Res_patchwork_drone',
         drone_stats=DRONE + ['max_velocity=6', 'max_walk_velocity=6', 'angle_velocity=1.5']),
]

# table -> [(new row, the row it copies, its changes)], applied in this order
ROWS = {
    'ammunition': [(KIT, 'heavy_sg', ['cost_recharge=200', 'ammo_class=none', 'comments=UAV kit (Spawn Framework)'])],
    'guns': [('uav_kit_launcher', 'gl_m6', [f'ammunition={KIT}:1', 'comment=UAV kit launcher (Spawn Framework)'])],
    'gun_mounts': [('uav_kit_mount', 'stryker_grenade_launcher_1', ['gun=uav_kit_launcher']),
                   ('uav_kit_mount_van', 'res_van_passenger_gun6', ['gun=uav_kit_launcher'])],
    'humans': [(u['drone'], 'Lgn_swarm', u['drone_stats']) for u in UNITS],
    'squads': [(u['squad'], 'Lgn_swarm', [f'members={u["drone"]}:1', 'perks+=,noCorpse']) for u in UNITS],
    'cars': [(u['van'], u['van_from'], u['van_stats']) for u in UNITS],
}
SKIRMISH = {'resistance': 'Res_van', 'founders': 'Fnd_HEMTT_ammunition'}  # faction -> the row the vans go after

# The vans' F4 look: pictures named with the mod's own prefix (no other mod's file can be in the way), tooltip keys
# of at most 15 characters (the plugin writes them into the game's string in place).
PICTURES = 'pwuav'
DRONES_ICON = 'textures/ui/pictures/tech_pic/infantry_icons_big/lgn_drones.dds'
TIP, TIP_RECALL = 'pwuav_deploy', 'pwuav_recall'
LOOK = {
    'spawn_icon': fw.picture_name(f'{PICTURES}_ready.dds'),
    'spawn_icon_cooldown': fw.picture_name(f'{PICTURES}_cooldown.dds'),
    'spawn_icon_recall': fw.picture_name(f'{PICTURES}_recall.dds'),
    'spawn_tooltip': TIP,
    'spawn_tooltip_recall': TIP_RECALL,
}
# locale/strres.xml rows (key, the game's row they copy, text); %s - the hotkey. With them, the framework's defaults
# (a whole strres.xml replaces another's in the standalone build).
STRINGS = fw.DEFAULT_STRINGS + [
    (TIP, 'order_unpack', 'Deploy drone (%s)'),
    (TIP + '_desc', 'order_unpack_desc', 'The vehicle stops and launches its drone. Each drone takes one UAV kit.'),
    (TIP + '_add', 'order_unpack_add', 'One drone at a time; the vehicle cannot move while deployed. A lost drone '
                                       'packs the vehicle up, and the next one is ready after a cooldown.'),
    (TIP_RECALL, 'order_pack', 'Recall drone (%s)'),
    (TIP_RECALL + '_desc', 'order_pack_desc', 'The vehicle packs up and switches to the mobile mode. Its drone '
                                              'self-destructs.'),
    (TIP_RECALL + '_add', 'order_pack_add', 'The next drone is ready after a cooldown.'),
]

# --test-vehicles (development builds): vehicles that try the framework's other settings, not part of the mod.
# Test_troop_van - untethered, two squads of the game's Resistance marksmen per launch, a UAV kit each, the
# framework's default F4 pictures (no spawn_icon*) and tooltip keys of its own.
TEST_TIP, TEST_TIP_RECALL = 'test_launch', 'test_recall'
TEST_UNITS = [
    dict(van='Test_troop_van', van_from='Res_van', van_name='Test Troop Van (untethered)',
         van_stats=F4 + ['gun_mounts=res_van_passenger_gun1,res_van_passenger_gun2,res_van_passenger_gun3,'
                         'res_van_passenger_gun4,res_van_passenger_gun5,uav_kit_mount_van',
                         f'ammunition={KIT}:{KITS}'],
         columns=f'spawn_squad=Res_marksmans;spawn_ammo={KIT};spawn_cooldown=30;spawn_tethered=false;'
                 f'spawn_count=2;spawn_tooltip={TEST_TIP};spawn_tooltip_recall={TEST_TIP_RECALL}'),
]
TEST_STRINGS = [
    (TEST_TIP, 'order_unpack', 'Launch squads (%s)'),
    (TEST_TIP + '_desc', 'order_unpack_desc', 'Test: the vehicle launches two marksman squads and packs up again.'),
    (TEST_TIP + '_add', 'order_unpack_add', 'Untethered: the squads are on their own. Next launch after the cooldown.'),
    (TEST_TIP_RECALL, 'order_pack', 'Mobile mode (%s)'),
    (TEST_TIP_RECALL + '_desc', 'order_pack_desc', 'The vehicle packs up. Its squads stay.'),
    (TEST_TIP_RECALL + '_add', 'order_pack_add', 'Test vehicle of the Spawn Framework.'),
]

MOD_JSON = {
    'name': 'Patchwork UAV',
    'description': ('Two vans that keep a recon drone in the air while deployed (F4): the Integrators\' Recon Van '
                    'and the Resistance\'s Patchwork UAV Van, a drone put together from Legion scrap. Each carries '
                    f'{KITS} UAV kits, one per drone. Requires the Spawn Framework (plugin and mod).'),
    'icon': 'basis/mod_icon.dds',
}


def tool(script, *args):
    """Run a script from mod/tools with this Python; an error stops the build."""
    env = dict(os.environ, PYTHONIOENCODING='utf-8')
    subprocess.run([sys.executable, os.path.join(TOOLS, script), *args], check=True, env=env)


def clone_rows(src, out, rows):
    """Copy the game's table src to out and add the cloned rows to it."""
    shutil.copy(src, out)
    for new, row, changes in rows:
        tool('patch_table.py', out, out + '.tmp', row, '--clone', new, *changes)
        os.replace(out + '.tmp', out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', required=True)
    ap.add_argument('--out', required=True, help='the mod folder, e.g. .../mods/Patchwork_UAV (recreated)')
    ap.add_argument('--cache', help='where to unpack the game files (default <out>_game_src)')
    ap.add_argument('--version', default='dev')
    ap.add_argument('--test-vehicles', action='store_true', help='add the test vehicles (TEST_UNITS)')
    a = ap.parse_args()
    tests = TEST_UNITS if a.test_vehicles else []
    cache = a.cache or a.out + '_game_src'
    shutil.rmtree(a.out, ignore_errors=True)
    g = Game(a.game)
    S = lambda layer, rel: g.extract(layer, rel, os.path.join(cache, layer))[0]  # noqa: E731

    print('== tables')
    species = os.path.join(a.out, 'basis', 'scripts', 'species')
    os.makedirs(species)
    tables = dict(ROWS, cars=ROWS['cars'] + [(t['van'], t['van_from'], t['van_stats']) for t in tests])
    for table, rows in tables.items():
        clone_rows(S('base', f'scripts/species/{table}.xml'), os.path.join(species, f'{table}.xml'), rows)
    cars = os.path.join(species, 'cars.xml')
    look = ';'.join(f'{k}={v}' for k, v in LOOK.items())
    tool('add_columns.py', cars, cars, ','.join(fw.SPAWN_COLUMNS),
         *[f'{u["van"]}:spawn_squad={u["squad"]};spawn_ammo={KIT};spawn_cooldown=60;{look}' for u in UNITS],
         *[f'{t["van"]}:{t["columns"]}' for t in tests])

    print('== skirmish armies')
    edits, anchors = [], dict(SKIRMISH)
    for u in UNITS:
        for faction, anchor in anchors.items():
            edits.append(f'add-car:{faction}:{anchor}:{u["van"]}')
            anchors[faction] = u['van']
    for t in tests:  # the Resistance's army only
        edits.append(f'add-car:resistance:{anchors["resistance"]}:{t["van"]}')
        anchors['resistance'] = t['van']
    tool('patch_army_toml.py', S('base', 'scripts/default_army.toml'),
         os.path.join(a.out, 'basis', 'scripts', 'default_army.toml'), *edits)

    print('== F4 pictures')
    pictures = os.path.join(a.out, 'basis', *fw.PICTURES_DIR.split('/'))
    os.makedirs(pictures)
    buttons.make(S('base', fw.UNPACK_BUTTON), front_drone(buttons.load(S('base', DRONES_ICON))[0]),
                 os.path.join(pictures, PICTURES))

    print('== names')
    loc = os.path.join(a.out, 'localization', 'en', 'basis_en', 'locale')
    os.makedirs(loc)
    clone_rows(S('loc_en', 'locale/cars.xml'), os.path.join(loc, 'cars.xml'),
               [(u['van'], u['van_from'], [f'content={u["van_name"]}']) for u in UNITS + tests])
    clone_rows(S('loc_en', 'locale/squads.xml'), os.path.join(loc, 'squads.xml'),
               [(u['squad'], 'Lgn_swarm', [f'content={u["squad_name"]}']) for u in UNITS])
    clone_rows(S('loc_en', 'locale/strres.xml'), os.path.join(loc, 'strres.xml'),
               [(key, row, [f'content={text}']) for key, row, text in STRINGS + (TEST_STRINGS if tests else [])])

    # The MODS menu lists only mods with an icon; until the mod has its own, the game's example mod's.
    shutil.copy(os.path.join(a.game, 'mods', 'mod_example_1', 'basis', 'mod_icon.dds'),
                os.path.join(a.out, 'basis', 'mod_icon.dds'))
    info = dict(MOD_JSON, description=f'Version {a.version}.\n' + MOD_JSON['description'])
    with open(os.path.join(a.out, 'mod.json'), 'w', encoding='utf-8') as f:
        json.dump(info, f, indent=4)
    print(f'Patchwork UAV -> {a.out}')


if __name__ == '__main__':
    main()
