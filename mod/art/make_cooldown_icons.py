"""Cooldown pictures of a command panel button, made the way the game made its own:

    python mod/art/make_cooldown_icons.py <icon.dds> <out_n.dds> <out_o.dds>

A timer ability's button (rush.txt, SpawnSmoke.txt...) draws a progress bar over itself while the ability cools
down: `image` - <icon>_cooldown_n, the icon itself; `progress_image` - <icon>_cooldown_o, a dim grey copy of it,
shown over the part of the wait still to go. F4 (unpack) has no such pictures; these are made from its icon with
the same recipe, measured on the game's ability_charge / ability_smoke pairs (mean error 1-5 of 255 inside):
    _cooldown_n: each channel ~ the icon's (R 0.988x+2.5, G 1.043x-9.3, B 1.103x-23.2), alpha as the icon's;
    _cooldown_o: grey 0.262 x luminance + 33.9 in every channel, alpha 230;
    both: the icon's frame dropped - its outer 5 pixels transparent (the game's drop 4; the 5th is the icon's rim).
The pictures are uncompressed 32-bit DDS (R in the low byte); the output keeps the icon's own 128-byte header.
"""
import sys

BORDER = 5
N_FIT = ((0.988, 2.5), (1.043, -9.3), (1.103, -23.2))
O_GREY, O_ALPHA = (0.262, 33.9), 230


def clamp(v):
    return max(0, min(255, int(round(v))))


def main(src, out_n, out_o):
    data = open(src, 'rb').read()
    header, px = data[:128], data[128:]
    height, width = int.from_bytes(data[12:16], 'little'), int.from_bytes(data[16:20], 'little')
    if len(px) < width * height * 4 or int.from_bytes(data[88:92], 'little') != 32:
        sys.exit(f'{src}: not an uncompressed 32-bit DDS')
    n, o = bytearray(), bytearray()
    for y in range(height):
        for x in range(width):
            r, g, b, a = px[4 * (y * width + x):4 * (y * width + x) + 4]
            edge = min(x, y, width - 1 - x, height - 1 - y) < BORDER
            if edge or a == 0:
                n += bytes(4)
                o += bytes(4)
                continue
            n += bytes([clamp(k * c + d) for c, (k, d) in zip((r, g, b), N_FIT)] + [a])
            grey = clamp(O_GREY[0] * (0.299 * r + 0.587 * g + 0.114 * b) + O_GREY[1])
            o += bytes([grey, grey, grey, O_ALPHA])
    for path, pixels in ((out_n, n), (out_o, o)):
        with open(path, 'wb') as f:
            f.write(header + bytes(pixels) + px[width * height * 4:])
        print(f'{path}: {width}x{height}')


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
