"""A command panel button with a drone on it, from the game's own pictures:

    python mod/art/make_uav_button.py <button.dds> <drones.dds> <out.dds> [--recall]

<button.dds> - a 76x76 button of the panel (F4's buttons_unpack_n): its frame and its background are kept, its icon
is painted over with the background; <drones.dds> - the swarm drones' unit icon (tech_pic/infantry_icons_big/
lgn_drones, three drones in the same style as the buttons' icons: light blue, dark outline). The front drone is cut
out of it and put in the middle - the deploy button. --recall: the drone higher up with an arrow down under it, drawn
in the same colours - the pack-up button (the drone called back). The result is a button picture for the Spawn
Framework's progress bars (art/make_cooldown_icons.py makes the deploy button's cooldown pair) - the game's own files
are only read.

The button's background is a vertical gradient, the same across each row: inside the frame (BORDER pixels from the
edge) every pixel takes the colour of its row at the frame's inner edge. The pictures are uncompressed 32-bit DDS
(R in the low byte); the output keeps the button's 128-byte header.
"""
import sys

from PIL import Image, ImageDraw, ImageFilter

BORDER = 5     # the button's frame and glow, kept as they are
ICON = 50      # the drone's height on the button, pixels (the box icon is about as big)
RECALL_ICON = 36
# The front drone of lgn_drones (148x92): drawn whole, over the other two. Its rows above FRONT_TOP_ROWS are its own;
# below them only its tail, the columns FRONT_TAIL. Its outline: OUTLINE pixels around its fill, from the picture.
DRONES_SIZE = (148, 92)
FRONT_TOP_ROWS = 43
FRONT_TAIL = (43, 54, 66, 82)  # rows from, to; columns from, to
OUTLINE = 2
FILL = (176, 208, 220, 255)    # the arrow: the drone's light blue and dark outline
DARK = (26, 42, 51, 255)


def load(path):
    """(image, header) of an uncompressed 32-bit DDS."""
    data = open(path, 'rb').read()
    h, w = int.from_bytes(data[12:16], 'little'), int.from_bytes(data[16:20], 'little')
    if int.from_bytes(data[88:92], 'little') != 32:
        sys.exit(f'{path}: not an uncompressed 32-bit DDS')
    return Image.frombytes('RGBA', (w, h), data[128:128 + w * h * 4]), data[:128]


def front_drone(drones):
    """The front drone of the swarm drones' icon, cropped."""
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


def arrow(width, height):
    """An arrow pointing down, light blue with a dark outline."""
    img = Image.new('RGBA', (width + 4, height + 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    shaft = width // 3
    head = height * 5 // 9
    cx = (width + 4) // 2
    pts = [(cx - shaft // 2, 2), (cx + shaft // 2, 2), (cx + shaft // 2, 2 + height - head), (width + 1, 2 + height - head),
           (cx, height + 1), (2, 2 + height - head), (cx - shaft // 2, 2 + height - head)]
    d.polygon(pts, fill=FILL, outline=DARK, width=2)
    return img


def main(button_path, drones_path, out_path, recall=False):
    button, header = load(button_path)
    drones, _ = load(drones_path)
    w, h = button.size
    px = button.load()
    for y in range(BORDER, h - BORDER):  # the icon away: each row takes its background colour
        fill = px[BORDER, y]
        for x in range(BORDER, w - BORDER):
            px[x, y] = fill
    icon = front_drone(drones)
    size = RECALL_ICON if recall else ICON
    scale = size / icon.height
    icon = icon.resize((max(1, round(icon.width * scale)), size), Image.LANCZOS)
    if recall:
        down = arrow(26, 20)
        top = (h - icon.height - down.height + 2) // 2
        button.alpha_composite(icon, ((w - icon.width) // 2, top))
        button.alpha_composite(down, ((w - down.width) // 2, top + icon.height - 2))
    else:
        button.alpha_composite(icon, ((w - icon.width) // 2, (h - icon.height) // 2))
    with open(out_path, 'wb') as f:
        f.write(header + button.tobytes())
    print(f'{out_path}: {w}x{h}, drone {icon.width}x{icon.height}{" + arrow" if recall else ""}')


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--recall']
    if len(args) != 3:
        sys.exit(__doc__)
    main(*args, recall='--recall' in sys.argv[1:])
