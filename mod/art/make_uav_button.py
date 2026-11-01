"""A command panel button with drones on it, from the game's own pictures:

    python mod/art/make_uav_button.py <button.dds> <drones.dds> <out.dds>

<button.dds> - a 76x76 button of the panel (F4's buttons_unpack_n): its frame and its background are kept, its icon
is painted over with the background; <drones.dds> - the swarm drones' unit icon (tech_pic/infantry_icons_big/
lgn_drones), drawn in the same style as the buttons' icons (light blue, dark outline): the drones are cut out and
put in the middle. The result is a button picture for the Spawn Framework's cooldown bar (art/make_cooldown_icons.py
makes its cooldown pair) - the game's own files are only read.

The button's background is a vertical gradient, the same across each row: inside the frame (BORDER pixels from the
edge) every pixel takes the colour of its row at the frame's inner edge. The pictures are uncompressed 32-bit DDS
(R in the low byte); the output keeps the button's 128-byte header.
"""
import sys

from PIL import Image

BORDER = 5     # the button's frame and glow, kept as they are
ICON = 54      # the drones' size on the button, pixels (the box icon is about as big)


def load(path):
    """(image, header) of an uncompressed 32-bit DDS."""
    data = open(path, 'rb').read()
    h, w = int.from_bytes(data[12:16], 'little'), int.from_bytes(data[16:20], 'little')
    if int.from_bytes(data[88:92], 'little') != 32:
        sys.exit(f'{path}: not an uncompressed 32-bit DDS')
    return Image.frombytes('RGBA', (w, h), data[128:128 + w * h * 4]), data[:128]


def main(button_path, drones_path, out_path):
    button, header = load(button_path)
    drones, _ = load(drones_path)
    w, h = button.size
    px = button.load()
    for y in range(BORDER, h - BORDER):  # the icon away: each row takes its background colour
        fill = px[BORDER, y]
        for x in range(BORDER, w - BORDER):
            px[x, y] = fill
    icon = drones.crop(drones.getbbox())
    scale = ICON / max(icon.size)
    icon = icon.resize((max(1, round(icon.width * scale)), max(1, round(icon.height * scale))), Image.LANCZOS)
    button.alpha_composite(icon, ((w - icon.width) // 2, (h - icon.height) // 2))
    with open(out_path, 'wb') as f:
        f.write(header + button.tobytes())
    print(f'{out_path}: {w}x{h}, drones {icon.width}x{icon.height}')


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
