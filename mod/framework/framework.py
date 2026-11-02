"""The Spawn Framework mod's pieces, shared by its build (build_framework_mod.py) and the builds of the mods made on it
(Patchwork UAV's):

  * F4's layout: the game's scripts/ui/AbilityButtons/pack.txt with two progress bars added - the only file every
    framework vehicle needs, so only the framework mod ships it;
  * the default F4 pictures and tooltips, for a vehicle whose row names none of its own;
  * the framework's cars.xml columns (the plugin's core/config.h).

The plugin (game_dll/panel.cpp) shows the bars on the framework's vehicles only, with each vehicle's own pictures;
the bars' pictures in pack.txt are the defaults.
"""

# The columns a vehicle of the framework may have in cars.xml / tanks.xml (core/config.h).
SPAWN_COLUMNS = ['spawn_squad', 'spawn_ammo', 'spawn_cooldown', 'spawn_icon', 'spawn_icon_cooldown',
                 'spawn_icon_recall', 'spawn_tooltip', 'spawn_tooltip_recall']

PICTURES_DIR = 'textures/ui/pictures/Main/buttons2'  # where F4's pictures live (the game's buttons_unpack_n...)
UNPACK_BUTTON = f'{PICTURES_DIR}/buttons_unpack_n.dds'  # the button the pictures are drawn on


def picture_name(file_name):
    """A picture of PICTURES_DIR as layouts and the spawn_icon* columns name it: main\\buttons2\\<file>."""
    return 'main\\buttons2\\' + file_name


# The defaults (panel.cpp kDefaultIcon...): the framework's pictures, made from the generic rifle squad's icon.
DEFAULT_PREFIX = 'spawn_framework'
DEFAULT_ICON_SOURCE = 'textures/ui/pictures/tech_pic/infantry_icons_big/gnrc_rifle.dds'
DEFAULT_TIP, DEFAULT_TIP_RECALL = 'spawn_deploy', 'spawn_recall'

# The default tooltips: locale/strres.xml rows, (key, the game's row they copy, text); %s - the hotkey. A mod made on
# the framework ships them too in its own strres.xml (whole files replace each other in the standalone build).
DEFAULT_STRINGS = [
    (DEFAULT_TIP, 'order_unpack', 'Deploy unit (%s)'),
    (DEFAULT_TIP + '_desc', 'order_unpack_desc', 'The vehicle stops and sends out its unit.'),
    (DEFAULT_TIP + '_add', 'order_unpack_add', 'One unit at a time; the vehicle cannot move while deployed. A lost '
                                               'unit packs the vehicle up, and the next one is ready after a cooldown.'),
    (DEFAULT_TIP_RECALL, 'order_pack', 'Recall unit (%s)'),
    (DEFAULT_TIP_RECALL + '_desc', 'order_pack_desc', 'The vehicle packs up and switches to the mobile mode. Its unit '
                                                      'self-destructs.'),
    (DEFAULT_TIP_RECALL + '_add', 'order_pack_add', 'The next unit is ready after a cooldown.'),
]

# F4's layout with the bars: "progress_bar" over the deploy button (its picture, filled with the cooldown picture
# while the wait runs), "progress_bar_2" over the pack-up one. Drawn over the buttons, they take no clicks.
PACK_LAYOUT = 'scripts/ui/AbilityButtons/pack.txt'


def progress_bar(name, tip, image, progress_image):
    return [
        f'{name}\tprogress_bar\t 0, 0, 76, 76\t\t{tip}\t\t',
        '\t\t\t\t\ttooltip_type\tsimple',
        '\t\t\t\t\tenabled\ttrue',
        '\t\t\t\t\tscaled\tnone',
        '\t\t\t\t\timage_type\tnormal',
        '\t\t\t\t\tindent_tb\tindent_top 0 indent_bottom 0',
        '\t\t\t\t\tindent_lr\tindent_left 0 indent_right 0',
        '\t\t\t\t\tis_interactable\tfalse',
        f'\t\t\t\t\timage\t{image}',
        f'\t\t\t\t\tprogress_image\t{progress_image}',
        '\t\t\t\t\tdirection\tvertical',
        '\t\t\t\t\tfont\tarial10b.fnt',
        '\t\t\t\t\tcolor\t0xffFFFFFF',
        '\t\t\t\t\tinverted\ttrue',
    ]


def pack_layout(game_layout):
    """The game's pack.txt (its text) with the framework's bars added."""
    if 'progress_bar' in game_layout:
        raise SystemExit(f'{PACK_LAYOUT} already has a progress bar - check what the game changed')
    nl = '\r\n' if '\r\n' in game_layout else '\n'
    p = DEFAULT_PREFIX
    bars = (progress_bar('progress_bar', 'order_unpack', picture_name(f'{p}_ready.dds'),
                         picture_name(f'{p}_cooldown.dds'))
            + progress_bar('progress_bar_2', 'order_pack', picture_name(f'{p}_recall.dds'),
                           picture_name(f'{p}_recall.dds')))
    return game_layout.rstrip('\r\n') + nl + nl.join(bars) + nl
