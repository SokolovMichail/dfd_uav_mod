"""Builds the probe data mod for SpawnProbe.dll (Phase 0) from the installed game's current files:

    python mod/probe/build_probe_mod.py --game <game folder> --out <mods/SpawnProbe>

Two test vehicles for skirmish, both with the F4 button (abilities=ammunition_recharge, nothing to hand out):
- Probe_uav_van: a copy of the Resistance van (Res_van), no kits (no gun to load them) - spawns the scout swarm;
- Probe_gun_van: a copy of the Integrators' gun truck (Int_guntruck) whose grenade launcher is replaced by a UAV kit
  launcher with 5 kits - spawns a one-drone bomb squad, a kit each time.
Both are added to the Resistance and Founders skirmish armies. No name, icon or shop - they show their sysnames.

The UAV kit (ammunition uav_kit) is a copy of the smoke round heavy_sg that costs 200 to restock and that nothing
is a target for (ammo_class none); its gun and mount are copies of the Stryker's smoke launcher. The bomb drone
(humans Probe_bomb_drone, squad Probe_bomb_uav: one drone, noCorpse) is a scout with a small blast (1-3:he60, 3 m)
and the bomb drones' destruction effect, so that blowing it up next to the van does little harm.

The mod replaces the whole cars.xml and default_army.toml, so disable other mods that ship them (HEMTT Spares)
while probing.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, '..', 'tools')
sys.path.insert(0, TOOLS)
from gamefiles import Game  # noqa: E402

F4 = ['abilities=ammunition_recharge', 'capacity_ammo_recharger=0']
# table -> [(new row, the row it copies, its changes)], applied in this order
ROWS = {
    'ammunition': [('uav_kit', 'heavy_sg', ['cost_recharge=200', 'ammo_class=none', 'comments=UAV kit (probe)'])],
    'guns': [('uav_kit_launcher', 'gl_m6', ['ammunition=uav_kit:1', 'comment=UAV kit launcher (probe)'])],
    'gun_mounts': [('uav_kit_mount', 'stryker_grenade_launcher_1', ['gun=uav_kit_launcher'])],
    'humans': [('Probe_bomb_drone', 'Lgn_swarm', ['explode_damage=1-3:he60', 'explode_splash_radius=3',
                                                 'destroy_effect=destruction_swarm_explosive'])],
    'squads': [('Probe_bomb_uav', 'Lgn_swarm', ['members=Probe_bomb_drone:1', 'perks+=,noCorpse'])],
    'cars': [('Probe_uav_van', 'Res_van', F4),
             ('Probe_gun_van', 'Int_guntruck', F4 + ['gun_mounts=stryker_machine_gun,uav_kit_mount',
                                                     'ammunition=ammo_50cal:2000,uav_kit:5'])],
}
VEHICLES = [new for new, _, _ in ROWS['cars']]


def tool(script, *args):
    """Run a script from mod/tools with this Python; an error stops the build."""
    env = dict(os.environ, PYTHONIOENCODING='utf-8')
    subprocess.run([sys.executable, os.path.join(TOOLS, script), *args], check=True, env=env)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', required=True)
    ap.add_argument('--out', required=True, help='the mod folder, e.g. .../mods/SpawnProbe (recreated)')
    a = ap.parse_args()

    cache = os.path.join(a.out + '_game_src')
    shutil.rmtree(a.out, ignore_errors=True)
    g = Game(a.game)
    army, _ = g.extract('base', 'scripts/default_army.toml', cache)

    species = os.path.join(a.out, 'basis', 'scripts', 'species')
    os.makedirs(species)
    for table, rows in ROWS.items():
        src, _ = g.extract('base', f'scripts/species/{table}.xml', cache)
        out = os.path.join(species, f'{table}.xml')
        shutil.copy(src, out)
        for new, row, changes in rows:
            tool('patch_table.py', out, out + '.tmp', row, '--clone', new, *changes)
            os.replace(out + '.tmp', out)
    # each new vehicle goes after the previous one: Res_van / Fnd_HEMTT_ammunition, then Probe_uav_van
    edits, anchors = [], {'resistance': 'Res_van', 'founders': 'Fnd_HEMTT_ammunition'}
    for van in VEHICLES:
        for faction, anchor in anchors.items():
            edits.append(f'add-car:{faction}:{anchor}:{van}')
            anchors[faction] = van
    tool('patch_army_toml.py', army, os.path.join(a.out, 'basis', 'scripts', 'default_army.toml'), *edits)
    # The MODS menu lists only mods with an icon; the probe borrows the game's example mod's.
    shutil.copy(os.path.join(a.game, 'mods', 'mod_example_1', 'basis', 'mod_icon.dds'),
                os.path.join(a.out, 'basis', 'mod_icon.dds'))
    with open(os.path.join(a.out, 'mod.json'), 'w', encoding='utf-8') as f:
        json.dump({'name': 'Spawn Probe',
                   'description': 'Test vehicles Probe_uav_van and Probe_gun_van for SpawnProbe.dll (skirmish).',
                   'icon': 'basis/mod_icon.dds'}, f, indent=4)
    print(f'probe mod -> {a.out}')


if __name__ == '__main__':
    main()
