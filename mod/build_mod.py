"""Builds the example mod Patchwork UAV (mods\\Patchwork_UAV) from the installed game's current files:

    python mod/build_mod.py --game <game folder> --out <mods/Patchwork_UAV> [--cache <dir>] [--version 0.1]

Two vans that, deployed (F4), keep one recon drone of their own in the air - the Spawn Framework's example. Each
carries 5 UAV kits (an ammunition of its own, 200 to restock each) and spends one per drone; the drone dies when
its van packs up or is destroyed, and the next one waits 60 s after it.

- Integrators: Int_recon_van, on the Integrators' gun truck (Int_guntruck): the smoke launcher is a UAV kit
  launcher. Its drone, Int_recon_uav, is a swarm scout a little slower than the Legion's.
- Resistance / Founders: Res_patchwork_van, the "rusty" one, on the Resistance van (Res_van): one of its six
  passenger mounts holds the kit launcher. Its drone, Res_patchwork_uav, is put together from scrap: slow.

The game files are taken fresh every time (gamefiles.py: the last archive or folder of the layer that has the
file), so after a game update a rebuild is enough. The base layer only for now (skirmish, main campaign).

Without the Spawn Framework plugin the vans are ordinary vans with a launcher that never fires and the F4 button
of a supply vehicle with nothing to hand out.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, 'tools')
sys.path.insert(0, TOOLS)
from gamefiles import Game  # noqa: E402

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
# The framework's columns (core/config.h), added to cars.xml.
SPAWN_COLUMNS = ['spawn_squad', 'spawn_ammo', 'spawn_cooldown']
SKIRMISH = {'resistance': 'Res_van', 'founders': 'Fnd_HEMTT_ammunition'}  # faction -> the row the vans go after

# F4's button layout with a progress bar added: the Spawn Framework fills it while a vehicle's cooldown runs (and
# keeps it hidden otherwise), the way the timer abilities' buttons (rush.txt) show theirs. The game's pack.txt has
# only the two buttons. The bar is drawn over them and takes no clicks. A framework file: it changes every F4 button
# (shown only for the framework's vehicles); later it belongs in a framework mini-mod.
PACK_LAYOUT = 'scripts/ui/AbilityButtons/pack.txt'
PACK_PROGRESS_BAR = [
    'progress_bar\tprogress_bar\t 0, 0, 76, 76\t\torder_unpack\t\t',
    '\t\t\t\t\ttooltip_type\tsimple',
    '\t\t\t\t\tenabled\ttrue',
    '\t\t\t\t\tscaled\tnone',
    '\t\t\t\t\timage_type\tnormal',
    '\t\t\t\t\tindent_tb\tindent_top 0 indent_bottom 0',
    '\t\t\t\t\tindent_lr\tindent_left 0 indent_right 0',
    '\t\t\t\t\tis_interactable\tfalse',
    '\t\t\t\t\timage\tmain\\buttons2\\buttons_unpack_p.dds',
    '\t\t\t\t\tprogress_image\tmain\\buttons2\\buttons_unpack_n.dds',
    '\t\t\t\t\tdirection\tvertical',
    '\t\t\t\t\tfont\tarial10b.fnt',
    '\t\t\t\t\tcolor\t0xffFFFFFF',
    '\t\t\t\t\tinverted\ttrue',
]

MOD_JSON = {
    'name': 'Patchwork UAV',
    'description': ('Two vans that keep a recon drone in the air while deployed (F4): the Integrators\' Recon Van '
                    'and the Resistance\'s Patchwork UAV Van, a drone put together from Legion scrap. Each carries '
                    f'{KITS} UAV kits, one per drone. Requires the Spawn Framework.'),
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
    a = ap.parse_args()
    cache = a.cache or a.out + '_game_src'
    shutil.rmtree(a.out, ignore_errors=True)
    g = Game(a.game)
    S = lambda layer, rel: g.extract(layer, rel, os.path.join(cache, layer))[0]  # noqa: E731

    print('== tables')
    species = os.path.join(a.out, 'basis', 'scripts', 'species')
    os.makedirs(species)
    for table, rows in ROWS.items():
        clone_rows(S('base', f'scripts/species/{table}.xml'), os.path.join(species, f'{table}.xml'), rows)
    cars = os.path.join(species, 'cars.xml')
    tool('add_columns.py', cars, cars, ','.join(SPAWN_COLUMNS),
         *[f'{u["van"]}:spawn_squad={u["squad"]};spawn_ammo={KIT};spawn_cooldown=60' for u in UNITS])

    print('== skirmish armies')
    edits, anchors = [], dict(SKIRMISH)
    for u in UNITS:
        for faction, anchor in anchors.items():
            edits.append(f'add-car:{faction}:{anchor}:{u["van"]}')
            anchors[faction] = u['van']
    tool('patch_army_toml.py', S('base', 'scripts/default_army.toml'),
         os.path.join(a.out, 'basis', 'scripts', 'default_army.toml'), *edits)

    print('== F4 button layout')
    layout = open(S('base', PACK_LAYOUT), encoding='utf-8', newline='').read()
    if 'progress_bar' in layout:
        sys.exit(f'{PACK_LAYOUT} already has a progress bar - check what the game changed')
    nl = '\r\n' if '\r\n' in layout else '\n'
    layout = layout.rstrip('\r\n') + nl + nl.join(PACK_PROGRESS_BAR) + nl
    out_layout = os.path.join(a.out, 'basis', *PACK_LAYOUT.split('/'))
    os.makedirs(os.path.dirname(out_layout), exist_ok=True)
    with open(out_layout, 'w', encoding='utf-8', newline='') as f:
        f.write(layout)
    print(f'{PACK_LAYOUT}: + progress_bar')

    print('== names')
    loc = os.path.join(a.out, 'localization', 'en', 'basis_en', 'locale')
    os.makedirs(loc)
    clone_rows(S('loc_en', 'locale/cars.xml'), os.path.join(loc, 'cars.xml'),
               [(u['van'], u['van_from'], [f'content={u["van_name"]}']) for u in UNITS])
    clone_rows(S('loc_en', 'locale/squads.xml'), os.path.join(loc, 'squads.xml'),
               [(u['squad'], 'Lgn_swarm', [f'content={u["squad_name"]}']) for u in UNITS])

    # The MODS menu lists only mods with an icon; until the mod has its own, the game's example mod's.
    shutil.copy(os.path.join(a.game, 'mods', 'mod_example_1', 'basis', 'mod_icon.dds'),
                os.path.join(a.out, 'basis', 'mod_icon.dds'))
    info = dict(MOD_JSON, description=f'Version {a.version}.\n' + MOD_JSON['description'])
    with open(os.path.join(a.out, 'mod.json'), 'w', encoding='utf-8') as f:
        json.dump(info, f, indent=4)
    print(f'Patchwork UAV -> {a.out}')


if __name__ == '__main__':
    main()
