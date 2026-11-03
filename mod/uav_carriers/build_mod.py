"""Builds UAV Carriers (mods\\UAV_Carriers), a mod made on the Spawn Framework, from the installed game's current
files:

    python mod/uav_carriers/build_mod.py --game <game folder> --out <mods/UAV_Carriers> [--cache <dir>] [--version 0.1]

Two UAV carriers that, deployed (F4), keep one recon drone of their own in the air. Each carries 5 UAV kits (an
ammunition of its own, 500 to restock each) and spends one per drone; the drone dies when its carrier packs up or is
destroyed, and the next one waits 60 s after it. They carry no passengers.

- Integrators: Int_uav_carrier ("UAV Carrier"), on the Integrators' gun truck (Int_guntruck): the smoke launcher is
  a UAV kit launcher. Its drone, Int_recon_uav, is a swarm scout a little slower than the Legion's.
- Resistance / Founders: Res_patchwork_uav_carrier ("Patchwork UAV Carrier"), on the Resistance van (Res_van): its
  passenger mounts give way to the kit launcher. Its drone, Res_patchwork_uav, is put together from scrap: slow and
  rusty, its eye glowing green.

What makes them the framework's is their cars.xml rows (the framework's columns, framework/framework.py); F4's
layout comes with the Spawn Framework mod. Their F4 pictures and tooltips are the mod's own: the front drone of the
swarm drones' icon (art/front_drone.py, made into F4's pictures by the framework's art/buttons.py) and "Deploy
drone" / "Recall drone".

The game files are taken fresh every time (gamefiles.py: the last archive or folder of the layer that has the
file), so after a game update a rebuild is enough. The base game (skirmish, main campaign) and both DLCs (LAYERS).

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
import rusty_drone  # noqa: E402

KIT = 'uav_kit'
KITS = 5
# A drone is a single-use, fragile scout with no range limit: eyes anywhere without risking anyone, but pricey, and
# its carrier cannot move while it flies. Its kit's restock cost (the game's: smoke rounds 200).
KIT_COST = 500
# The kit's own ammunition pictures (art/kit_icons.py: the drone in the style of the game's, made on the smoke
# round's) - textures/ui/pictures/Ammunition/uavc_kit[_o, _p, _off, _off_o, _off_p].dds - and its name and
# description in the ammunition panel (locale/ammunition.xml).
KIT_PICTURE = 'uavc_kit'
# The Patchwork UAV Carrier's drone is rusty (art/rusty_drone.py): its own model, material and textures, made from
# the swarm drone's; the UAV Carrier's drone and the Legion's keep the clean ones.
RUSTY_DRONE = 'uavc_drone'
SWARM = dict(model='animations/new/skin/lgn_swarm.model', material='materials/vehicles/lgn_swarm.material',
             albedo='textures/vehicle/swarm_albedo.dds', rough='textures/vehicle/swarm_rough.dds',
             emission='textures/vehicle/swarm_emission.dds',
             rust='textures/buildings/rustmetal_albedo.dds')
KIT_LOOK_FROM = 'textures/ui/pictures/Ammunition/smoke_heavy'
KIT_TEXT = ['content=UAV kits', 'tooltip_name=UAV kits',  # tooltip_add: the game's (the supply cost, forbidding)
            'tooltip_desc=A recon drone, packed for launch: a deployed drone van (F4) spends one per drone. No gun '
            'fires it.']
# The F4 button (a supply component that hands out nothing: the framework hangs the deploy on it); no passengers
# (the crew drives as before: it is not a passenger - HEMTT Supply has none either).
F4 = ['abilities=ammunition_recharge', 'capacity_ammo_recharger=0', 'people_capacity=0']
# A recon drone: a swarm scout alone, no corpse, a small blast when it dies (its van packs up) - the bomb drones'
# death effect, a tenth of their blast.
DRONE = ['explode_damage=1-2:he30', 'explode_splash_radius=2', 'destroy_effect=destruction_swarm_explosive']

UNITS = [
    dict(van='Int_uav_carrier', van_from='Int_guntruck', van_name='UAV Carrier', faction='integrators',
         van_stats=F4 + ['gun_mounts=stryker_machine_gun,uav_kit_mount', f'ammunition=ammo_50cal:2000,{KIT}:{KITS}'],
         squad='Int_recon_uav', squad_name='Recon UAV', drone='Int_recon_drone',
         drone_stats=DRONE + ['max_velocity=9', 'max_walk_velocity=9']),
    dict(van='Res_patchwork_uav_carrier', van_from='Res_van', van_name='Patchwork UAV Carrier', faction='resistance',
         van_stats=F4 + ['gun_mounts=uav_kit_mount_van',  # no passengers to man the passenger mounts
                         f'ammunition={KIT}:{KITS}'],
         squad='Res_patchwork_uav', squad_name='Patchwork UAV', drone='Res_patchwork_drone',
         drone_stats=DRONE + ['max_velocity=6', 'max_walk_velocity=6', 'angle_velocity=1.5',
                              f'mesh={RUSTY_DRONE}']),  # rusty: its own model (art/rusty_drone.py)
]

# table -> [(new row, the row it copies, its changes)], applied in this order
ROWS = {
    'ammunition': [(KIT, 'heavy_sg', [f'cost_recharge={KIT_COST}', 'ammo_class=none', 'comments=UAV kit (Spawn Framework)',
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

# The encyclopedia's articles of the vans and their drones, in every layer's encyclopedia (&#10; - a new line, as the
# game's tables write it). The drones' rows are copies of the swarm scouts' (Lgn_swarm), of their carrier's faction,
# with a picture of one drone (ENCYCLOPEDIA_PICTURE).
COSTS = (f'Each drone takes one UAV kit; the carrier holds {KITS}, and a kit costs {KIT_COST} to restock. '
         'The next drone is ready 60 s after the last one is gone.')
ENCYCLOPEDIA = {
    'Int_uav_carrier': ('UAV Carrier', 'Integrators gun truck refitted as a drone carrier.&#10;&#10;Deployed (F4), it '
                        'keeps one recon drone in the air: eyes anywhere on the map, with no soldier at risk. While '
                        'it flies the drone, the truck cannot move; the drone self-destructs when the truck packs up, '
                        'and a lost drone packs the truck up.&#10;&#10;' + COSTS),
    'Res_patchwork_uav_carrier': ('Patchwork UAV Carrier', 'A Resistance van with a recon drone put together from '
                                  'Legion scrap. Slow, but it flies.&#10;&#10;Deployed (F4), it keeps the drone in '
                                  'the air: eyes anywhere on the map, with no fighter at risk. While it flies the '
                                  'drone, the van cannot move; the drone self-destructs when the van packs up, and a '
                                  'lost drone packs the van up.&#10;&#10;' + COSTS),
    'Int_recon_uav': ('Recon UAV', 'A Legion swarm scout, rebuilt by the Integrators to answer to them alone.&#10;&#10;'
                      'It has no range limit and no gun: it goes where no soldier should, and sees for them. It is '
                      'fragile and single-use - it self-destructs when its carrier packs up - and every drone costs '
                      'a pricey UAV kit.&#10;&#10;Only a deployed UAV Carrier can launch it.'),
    # The operating instructions: DFDRepairMod docs/Next_Mods_Handoff.md, "For the Nexus descriptions".
    'Res_patchwork_uav': ('Patchwork UAV', 'Ten wrecked Legion scouts went in, one working drone came out. Put '
                          'together from scrap. Flies. Sometimes.&#10;&#10;Operating instructions:&#10;'
                          '1. Deploy the van.&#10;2. Recite the Tech-Litany.&#10;3. Pray to the Omnissiah.&#10;'
                          '4. If the drone takes off, the blessing is granted. If not, repeat step 2 louder.'
                          '&#10;&#10;Warranty void for drones that revert to Legion factory settings.'
                          '&#10;&#10;Slow and rusty, its eye glowing green, it has no range limit and no gun: it '
                          'goes where no fighter should, and sees for them. It is fragile and single-use - it '
                          'self-destructs when its carrier packs up - and every drone costs a pricey UAV kit.'
                          '&#10;&#10;Only a deployed Patchwork UAV Carrier can launch it.'),
}
ENCYCLOPEDIA_PICTURE = 'uavc_drone'  # textures/ui/pictures/tech_pic/infantry_icons_big/uavc_drone.dds

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
PICTURES = 'uavc'
DRONES_ICON = 'textures/ui/pictures/tech_pic/infantry_icons_big/lgn_drones.dds'
TIP, TIP_RECALL = 'uavc_deploy', 'uavc_recall'
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
         van_stats=F4 + ['gun_mounts=uav_kit_mount_van', f'ammunition={KIT}:{KITS}'],
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
    'name': 'UAV Carriers',
    'description': ('Two UAV carriers that keep a recon drone in the air while deployed (F4): the Integrators\' UAV '
                    'Carrier and the Resistance\'s Patchwork UAV Carrier, its drone put together from Legion scrap. '
                    f'Each carries {KITS} UAV kits, one per drone, and no passengers. Requires the Spawn Framework '
                    '(plugin and mod).'),
    'icon': 'basis/mod_icon.dds',
}


def encyclopedia_picture(drone, size, margin=6):
    """One drone, as large as it fits, centred on a transparent picture of the swarm icon's size."""
    k = min((size[0] - 2 * margin) / drone.width, (size[1] - 2 * margin) / drone.height)
    one = drone.resize((round(drone.width * k), round(drone.height * k)), Image.LANCZOS)
    out = Image.new('RGBA', size, (0, 0, 0, 0))
    out.alpha_composite(one, ((size[0] - one.width) // 2, (size[1] - one.height) // 2))
    return out


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
    ap.add_argument('--out', required=True, help='the mod folder, e.g. .../mods/UAV_Carriers (recreated)')
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
                   [(u['van'], u['van_from'], ['presets=']) for u in UNITS] +  # not the gun trucks' variants
                   [(u['squad'], 'Lgn_swarm', ['presets=', f'faction={u["faction"]}',
                                               f'image=infantry_icons_big/{ENCYCLOPEDIA_PICTURE}']) for u in UNITS])
        loc = os.path.join(a.out, *L['loc_dir'].split('/'))
        os.makedirs(loc, exist_ok=True)
        articles = os.path.join(loc, f'{L["encyclopedia_loc"]}.xml')
        clone_rows(S(L['loc_layer'], f'locale/{L["encyclopedia_loc"]}.xml'), articles,
                   [(key, row, [f'name={ENCYCLOPEDIA[key][0]}', f'description={ENCYCLOPEDIA[key][1]}'])
                    for u in UNITS for key, row in ((u['van'], u['van_from']), (u['squad'], 'Lgn_swarm'))])

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

    print('== the drones\' encyclopedia picture')
    tech = os.path.join(a.out, 'basis', 'textures', 'ui', 'pictures', 'tech_pic', 'infantry_icons_big')
    os.makedirs(tech)
    drones, header = buttons.load(S('base', DRONES_ICON))
    buttons.save(os.path.join(tech, f'{ENCYCLOPEDIA_PICTURE}.dds'), encyclopedia_picture(drone, drones.size), header)

    print('== the UAV kit\'s pictures')
    ammo_pictures = os.path.join(a.out, 'basis', 'textures', 'ui', 'pictures', 'Ammunition')
    os.makedirs(ammo_pictures)
    smoke = {s: Image.open(S('base', f'{KIT_LOOK_FROM}{s}.dds')).convert('RGBA') for s in kit_icons.STATES}
    header = open(S('base', f'{KIT_LOOK_FROM}_off.dds'), 'rb').read()[:128]  # uncompressed 32-bit, 88x52
    kit_icons.make(drone, smoke, header, os.path.join(ammo_pictures, KIT_PICTURE))

    print('== the rusty drone')
    basis = os.path.join(a.out, 'basis')
    for d in ('animations/new/skin', 'materials/vehicles', 'textures/vehicle'):
        os.makedirs(os.path.join(basis, *d.split('/')), exist_ok=True)
    material = f'materials/vehicles/{RUSTY_DRONE}.material'
    model = rusty_drone.model_with_material(open(S('base', SWARM['model']), 'rb').read(), material)
    with open(os.path.join(basis, 'animations', 'new', 'skin', f'{RUSTY_DRONE}.model'), 'wb') as f:
        f.write(model)
    with open(os.path.join(basis, *material.split('/')), 'w', encoding='utf-8', newline='\n') as f:
        f.write(rusty_drone.material(open(S('base', SWARM['material']), encoding='utf-8').read(), RUSTY_DRONE))
    texture = lambda rel: Image.open(S('base', SWARM[rel])).convert('RGBA')  # noqa: E731
    albedo, rough = rusty_drone.rusty(texture('albedo'), texture('rough'), texture('rust'))
    albedo, emission = rusty_drone.green_eye(albedo, brightest=180), rusty_drone.green_eye(texture('emission'))
    for kind, image in (('albedo', albedo), ('rough', rough), ('emission', emission)):
        rusty_drone.write_dds(os.path.join(basis, 'textures', 'vehicle', f'{RUSTY_DRONE}_{kind}.dds'), image)

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
    print(f'UAV Carriers -> {a.out}')


if __name__ == '__main__':
    main()
