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

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.join(HERE, '..')
TOOLS = os.path.join(MOD, 'tools')
for p in (TOOLS, os.path.join(MOD, 'framework'), os.path.join(MOD, 'framework', 'art'), os.path.join(HERE, 'art')):
    sys.path.insert(0, p)
from gamefiles import Game  # noqa: E402
import buttons  # noqa: E402
import framework as fw  # noqa: E402
from front_drone import front_drone  # noqa: E402
import kit_icons  # noqa: E402

KIT = 'uav_kit'
KITS = 5
# The kit's own ammunition pictures (art/kit_icons.py: the drone in the style of the game's, made on the smoke
# round's) - textures/ui/pictures/Ammunition/pwuav_kit[_o, _p, _off, _off_o, _off_p].dds - and its name and
# description in the ammunition panel (locale/ammunition.xml).
KIT_PICTURE = 'pwuav_kit'
KIT_LOOK_FROM = 'textures/ui/pictures/Ammunition/smoke_heavy'
KIT_TEXT = ['content=UAV kits', 'tooltip_name=UAV kits',  # tooltip_add: the game's (the supply cost, forbidding)
            'tooltip_desc=A recon drone, packed for launch: a deployed drone van (F4) spends one per drone. No gun '
            'fires it.']
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
    'ammunition': [(KIT, 'heavy_sg', ['cost_recharge=200', 'ammo_class=none', 'comments=UAV kit (Spawn Framework)',
                                      f'image=Ammunition/{KIT_PICTURE}', f'image_grayed=Ammunition/{KIT_PICTURE}_off',
                                      f'image_denied=Ammunition/{KIT_PICTURE}_off'])],
    'guns': [('uav_kit_launcher', 'gl_m6', [f'ammunition={KIT}:1', 'comment=UAV kit launcher (Spawn Framework)'])],
    'gun_mounts': [('uav_kit_mount', 'stryker_grenade_launcher_1', ['gun=uav_kit_launcher']),
                   ('uav_kit_mount_van', 'res_van_passenger_gun6', ['gun=uav_kit_launcher'])],
    'humans': [(u['drone'], 'Lgn_swarm', u['drone_stats']) for u in UNITS],
    'squads': [(u['squad'], 'Lgn_swarm', [f'members={u["drone"]}:1', 'perks+=,noCorpse']) for u in UNITS],
    'cars': [(u['van'], u['van_from'], u['van_stats']) for u in UNITS],
}
SKIRMISH = {'resistance': 'Res_van', 'founders': 'Fnd_HEMTT_ammunition'}  # faction -> the row the vans go after

# The encyclopedia's articles of the vans (&#10; - a new line, as the game's tables write it).
ENCYCLOPEDIA = {
    'Int_recon_van': ('Recon Van', 'Integrators gun truck refitted as a drone carrier.&#10;&#10;Deployed (F4), it keeps '
                      'one recon drone in the air; the drone self-destructs when the van packs up, and a lost drone '
                      'packs the van up. Carries 5 UAV kits, one per drone; the next drone is ready 60 s after one '
                      'dies.'),
    'Res_patchwork_van': ('Patchwork UAV Van', 'A Resistance van with a recon drone put together from Legion scrap. '
                          'Slow, but it flies.&#10;&#10;Deployed (F4), it keeps the drone in the air; the drone '
                          'self-destructs when the van packs up, and a lost drone packs the van up. Carries 5 UAV '
                          'kits, one per drone; the next drone is ready 60 s after one dies.'),
}

# Where the vans are, one of each in every shop, as HEMTT Spares has it: the base game's campaign shops (not the
# test_shop_* and the demo's resistance_dlc_shop_demo), the first zone of the Resistance DLC's uprising, every shop
# of the Canada DLC. (shop, the cars cell's edit - {} the van).
BASE_SHOPS = ['taos_shop_1', 'taos_shop_2', 'taos_shop_3', 'albuquerque_shop', 'res_sup_base_0', 'res_sup_base_1',
              'vega_1_r', 'vega_1_m', 'tortuga_1', 'tortuga_2', 'tortuga_3', 'vega_2_r', 'vega_2_m',
              'resistance_camp', 'integrators_main_1', 'integrators_main_2', 'integrators_main_2a',
              'integrators_main_3', 'integrators_main_4', 'res_forward_post_0', 'integrators_scavengers_1',
              'integrators_scavengers_2', 'resistance_far_recon']
CANADA_SHOPS = ['camp_1_shop', 'camp_2_shop', 'camp_3_shop', 'niagara_shop', 'resistance_camp']

# The game's layers the vans go into. A DLC has its own cars, humans, squads, shops and encyclopedia (it takes the
# base game's ammunition, guns, mounts and names), so they go into each: the mod's folder has the layer's tree
# (dlc\<DLC>\basis\..., as the game's).
LAYERS = [
    dict(layer='base', dir='basis', tables=['ammunition', 'guns', 'gun_mounts', 'humans', 'squads', 'cars'],
         encyclopedia='encyclopedia', loc_layer='loc_en', encyclopedia_loc='encyclopedia_loc',
         loc_dir='localization/en/basis_en/locale', shops=[(s, 'cars,+={}') for s in BASE_SHOPS]),
    dict(layer='resistance', dir='dlc/Resistance/basis', tables=['humans', 'squads', 'cars'],
         encyclopedia='encyclopedia_dlc', loc_layer='resistance_loc_en', encyclopedia_loc='encyclopedia_dlc_loc',
         loc_dir='dlc/Resistance/localization/en/basis_en/locale', shops=[('sector_1_reward', 'cars+=,{}:1')]),
    dict(layer='canada', dir='dlc/Canada/basis', tables=['humans', 'squads', 'cars'],
         encyclopedia='encyclopedia_dlc', loc_layer='canada_loc_en', encyclopedia_loc='encyclopedia_dlc_loc',
         loc_dir='dlc/Canada/localization/en/basis_en/locale', shops=[(s, 'cars,+={}') for s in CANADA_SHOPS]),
]

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

    look = ';'.join(f'{k}={v}' for k, v in LOOK.items())
    for L in LAYERS:
        print(f'== {L["layer"]}: tables, shops, encyclopedia')
        species = os.path.join(a.out, *L['dir'].split('/'), 'scripts', 'species')
        os.makedirs(species)
        base = L['layer'] == 'base'
        for table in L['tables']:
            rows = ROWS[table] + ([(t['van'], t['van_from'], t['van_stats']) for t in tests]
                                  if table == 'cars' and base else [])  # the test vehicles: skirmish only
            path = os.path.join(species, f'{table}.xml')
            clone_rows(S(L['layer'], f'scripts/species/{table}.xml'), path, rows)
        cars = os.path.join(species, 'cars.xml')
        tool('add_columns.py', cars, cars, ','.join(fw.SPAWN_COLUMNS),
             *[f'{u["van"]}:spawn_squad={u["squad"]};spawn_ammo={KIT};spawn_cooldown=60;{look}' for u in UNITS],
             *([f'{t["van"]}:{t["columns"]}' for t in tests] if base else []))
        shops = os.path.join(species, 'shop_presets.xml')
        shutil.copy(S(L['layer'], 'scripts/species/shop_presets.xml'), shops)
        for shop, edit in L['shops']:
            for u in UNITS:
                tool('patch_table.py', shops, shops + '.tmp', shop, edit.format(u['van']))
                os.replace(shops + '.tmp', shops)
        encyclopedia = os.path.join(species, f'{L["encyclopedia"]}.xml')
        clone_rows(S(L['layer'], f'scripts/species/{L["encyclopedia"]}.xml'), encyclopedia,
                   [(u['van'], u['van_from'], ['presets=']) for u in UNITS])  # not the gun trucks' variants
        loc = os.path.join(a.out, *L['loc_dir'].split('/'))
        os.makedirs(loc, exist_ok=True)
        articles = os.path.join(loc, f'{L["encyclopedia_loc"]}.xml')
        clone_rows(S(L['loc_layer'], f'locale/{L["encyclopedia_loc"]}.xml'), articles,
                   [(u['van'], u['van_from'], [f'name={ENCYCLOPEDIA[u["van"]][0]}',
                                               f'description={ENCYCLOPEDIA[u["van"]][1]}']) for u in UNITS])

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
    drone = front_drone(buttons.load(S('base', DRONES_ICON))[0])
    buttons.make(S('base', fw.UNPACK_BUTTON), drone, os.path.join(pictures, PICTURES))

    print('== the UAV kit\'s pictures')
    ammo_pictures = os.path.join(a.out, 'basis', 'textures', 'ui', 'pictures', 'Ammunition')
    os.makedirs(ammo_pictures)
    smoke = {s: Image.open(S('base', f'{KIT_LOOK_FROM}{s}.dds')).convert('RGBA') for s in kit_icons.STATES}
    header = open(S('base', f'{KIT_LOOK_FROM}_off.dds'), 'rb').read()[:128]  # uncompressed 32-bit, 88x52
    kit_icons.make(drone, smoke, header, os.path.join(ammo_pictures, KIT_PICTURE))

    print('== names')
    loc = os.path.join(a.out, 'localization', 'en', 'basis_en', 'locale')
    os.makedirs(loc, exist_ok=True)
    clone_rows(S('loc_en', 'locale/cars.xml'), os.path.join(loc, 'cars.xml'),
               [(u['van'], u['van_from'], [f'content={u["van_name"]}']) for u in UNITS + tests])
    clone_rows(S('loc_en', 'locale/squads.xml'), os.path.join(loc, 'squads.xml'),
               [(u['squad'], 'Lgn_swarm', [f'content={u["squad_name"]}']) for u in UNITS])
    clone_rows(S('loc_en', 'locale/ammunition.xml'), os.path.join(loc, 'ammunition.xml'), [(KIT, 'heavy_sg', KIT_TEXT)])
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
