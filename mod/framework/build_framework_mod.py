"""Builds the Spawn Framework mod (mods\\Spawn_Framework) from the installed game's current files:

    python mod/framework/build_framework_mod.py --game <game folder> --out <mods/Spawn_Framework> [--cache <dir>]
                                                [--version 1.0]

The game-side half of the Spawn Framework, installed once with the plugin and shared by every mod made on it
(framework.py): F4's layout with the framework's two progress bars, and the default F4 pictures - the generic rifle
squad's icon - for a vehicle whose row names none of its own. The default tooltips' rows come with the mods made on
the framework (in the standalone build a strres.xml replaces another whole, so this mod ships none).
"""
import argparse
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
sys.path.insert(0, os.path.join(HERE, 'art'))
from gamefiles import Game  # noqa: E402
import buttons  # noqa: E402
import mod_art  # noqa: E402
import framework as fw  # noqa: E402

MOD_JSON = {
    'name': 'Spawn Framework',
    'description': ('Vehicles that keep a unit of their own while deployed (F4). The game-side part of the Spawn '
                    'Framework plugin: needed by the mods made on it, does nothing by itself.'),
    'icon': 'basis/mod_icon.dds',
}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', required=True)
    ap.add_argument('--out', required=True, help='the mod folder, e.g. .../mods/Spawn_Framework (recreated)')
    ap.add_argument('--cache', help='where to unpack the game files (default <out>_game_src)')
    ap.add_argument('--version', default='dev')
    a = ap.parse_args()
    cache = a.cache or a.out + '_game_src'
    shutil.rmtree(a.out, ignore_errors=True)
    g = Game(a.game)
    S = lambda rel: g.extract('base', rel, os.path.join(cache, 'base'))[0]  # noqa: E731

    print('== F4 layout')
    layout = fw.pack_layout(open(S(fw.PACK_LAYOUT), encoding='utf-8', newline='').read())
    out_layout = os.path.join(a.out, 'basis', *fw.PACK_LAYOUT.split('/'))
    os.makedirs(os.path.dirname(out_layout))
    with open(out_layout, 'w', encoding='utf-8', newline='') as f:
        f.write(layout)
    print(f'{fw.PACK_LAYOUT}: + progress_bar, progress_bar_2')

    print('== default F4 pictures')
    pictures = os.path.join(a.out, 'basis', *fw.PICTURES_DIR.split('/'))
    os.makedirs(pictures)
    buttons.make(S(fw.UNPACK_BUTTON), buttons.load(S(fw.DEFAULT_ICON_SOURCE))[0],
                 os.path.join(pictures, fw.DEFAULT_PREFIX))

    print('== the MODS menu icon')  # (the menu lists only mods with an icon)
    mod_art.save_dds(mod_art.render_framework(buttons.load(S(fw.UNPACK_BUTTON))[0],
                                              buttons.load(S(fw.DEFAULT_ICON_SOURCE))[0]),
                     os.path.join(a.out, 'basis', 'mod_icon.dds'))
    info = dict(MOD_JSON, description=f'Version {a.version}.\n' + MOD_JSON['description'])
    with open(os.path.join(a.out, 'mod.json'), 'w', encoding='utf-8') as f:
        json.dump(info, f, indent=4)
    print(f'Spawn Framework -> {a.out}')


if __name__ == '__main__':
    main()
