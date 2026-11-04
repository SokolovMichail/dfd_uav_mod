"""UAV Carriers' F4 icon: one drone, cut out of the swarm drones' unit icon (tech_pic/infantry_icons_big/
lgn_drones - three drones in the style of the panel's icons: light blue, dark outline). The front drone is drawn whole,
over the other two: its rows above FRONT_TOP_ROWS are its own; below them only its tail, the columns FRONT_TAIL. Its
outline: OUTLINE pixels around its fill, from the picture. The framework's art/buttons.py makes the F4 pictures of it.
"""
import sys

from PIL import Image, ImageFilter

DRONES_SIZE = (148, 92)
FRONT_TOP_ROWS = 43
FRONT_TAIL = (43, 54, 66, 82)  # rows from, to; columns from, to
OUTLINE = 2


def front_drone(drones):
    """The front drone of the swarm drones' icon (an RGBA image), cropped."""
    if drones.size != DRONES_SIZE:
        sys.exit(f'the drones icon is {drones.size}, not {DRONES_SIZE} - check what the game changed')
    w, h = drones.size
    px = drones.load()
    fill = Image.new('L', drones.size, 0)
    fp = fill.load()
    r0, r1, c0, c1 = FRONT_TAIL
    for y in range(h):
        for x in range(w):
            mine = y < FRONT_TOP_ROWS or (r0 <= y < r1 and c0 <= x < c1)
            r, _, _, a = px[x, y]
            if mine and a > 100 and r >= 40:  # the fill, not the outline
                fp[x, y] = 255
    keep = fill.filter(ImageFilter.MaxFilter(2 * OUTLINE + 1))
    out = Image.new('RGBA', drones.size, (0, 0, 0, 0))
    out.paste(drones, (0, 0), keep)
    return out.crop(out.getbbox())
