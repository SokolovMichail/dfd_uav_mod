"""F4 pictures for a vehicle of the Spawn Framework, made from the game's own pictures:

    python mod/framework/art/buttons.py <button.dds> <icon.dds> <out prefix>

writes <out prefix>_ready.dds, _cooldown.dds and _recall.dds - the three pictures a vehicle's row names in
spawn_icon, spawn_icon_cooldown and spawn_icon_recall. <button.dds> - a 76x76 button of the command panel (F4's
textures/ui/pictures/Main/buttons2/buttons_unpack_n.dds): its frame and background are kept, its own icon painted
over; <icon.dds> - what goes on the button, e.g. a unit icon (tech_pic/infantry_icons_big/...), cropped to what it
draws. A mod with an icon of its own to cut out first uses the functions (UAV Carriers' build does).

The pictures:
  ready     the button with the icon in the middle, coloured the way the game's timer buttons colour theirs (their
            _cooldown_n): F4's deploy button, and what the cooldown fills over;
  cooldown  a dim grey copy of it (the game's _cooldown_o): the part of the wait still to go;
  recall    the button with the icon higher up and an arrow down under it: F4's pack-up button (the squad called
            back).
The game's own files are only read. The pictures are uncompressed 32-bit DDS (R in the low byte); the outputs keep
the button's 128-byte header.

The colours, measured on the game's ability_charge / ability_smoke pairs (mean error 1-5 of 255 inside):
    ready:    each channel ~ the button's (R 0.988x+2.5, G 1.043x-9.3, B 1.103x-23.2), alpha as the button's;
    cooldown: grey 0.262 x luminance + 33.9 in every channel, alpha 230;
    both:     the frame dropped - the outer 5 pixels transparent (the game's drop 4; the 5th is the button's rim).
The button's background is a vertical gradient, the same across each row: inside the frame every pixel takes the
colour of its row at the frame's inner edge.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw

BORDER = 5          # the button's frame and glow
ICON = 50           # the icon's size on the deploy button, pixels (the game's box is about as big)
RECALL_ICON = 36    # and on the pack-up button, with the arrow under it
ARROW = (26, 20)
FILL = (176, 208, 220, 255)  # the arrow: the light blue and dark outline of the game's icons
DARK = (26, 42, 51, 255)
READY_FIT = ((0.988, 2.5), (1.043, -9.3), (1.103, -23.2))
GREY, GREY_ALPHA = (0.262, 33.9), 230


def load(path):
    """(image, header) of an uncompressed 32-bit DDS."""
    data = open(path, 'rb').read()
    h, w = int.from_bytes(data[12:16], 'little'), int.from_bytes(data[16:20], 'little')
    if int.from_bytes(data[88:92], 'little') != 32 or len(data) < 128 + w * h * 4:
        sys.exit(f'{path}: not an uncompressed 32-bit DDS')
    return Image.frombytes('RGBA', (w, h), data[128:128 + w * h * 4]), data[:128]


def save(path, image, header):
    with open(path, 'wb') as f:
        f.write(header + image.tobytes())
    print(f'{path}: {image.width}x{image.height}')


def blank(button):
    """The button with its own icon painted over: each row inside the frame takes its background colour."""
    out = button.copy()
    px = out.load()
    w, h = out.size
    for y in range(BORDER, h - BORDER):
        fill = px[BORDER, y]
        for x in range(BORDER, w - BORDER):
            px[x, y] = fill
    return out


def fit(icon, size):
    """The icon cropped to what it draws, its longer side `size` pixels."""
    icon = icon.crop(icon.getbbox())
    scale = size / max(icon.size)
    return icon.resize((max(1, round(icon.width * scale)), max(1, round(icon.height * scale))), Image.LANCZOS)


def arrow(width, height):
    """An arrow pointing down, light blue with a dark outline."""
    img = Image.new('RGBA', (width + 4, height + 4), (0, 0, 0, 0))
    shaft, head, cx = width // 3, height * 5 // 9, (width + 4) // 2
    pts = [(cx - shaft // 2, 2), (cx + shaft // 2, 2), (cx + shaft // 2, 2 + height - head),
           (width + 1, 2 + height - head), (cx, height + 1), (2, 2 + height - head), (cx - shaft // 2, 2 + height - head)]
    ImageDraw.Draw(img).polygon(pts, fill=FILL, outline=DARK, width=2)
    return img


def with_icon(button, icon):
    """The deploy button: the icon in the middle of the blank button."""
    out = blank(button)
    icon = fit(icon, ICON)
    out.alpha_composite(icon, ((out.width - icon.width) // 2, (out.height - icon.height) // 2))
    return out


def recall(button, icon):
    """The pack-up button: the icon higher up, an arrow down under it."""
    out = blank(button)
    icon, down = fit(icon, RECALL_ICON), arrow(*ARROW)
    top = (out.height - icon.height - down.height + 2) // 2
    out.alpha_composite(icon, ((out.width - icon.width) // 2, top))
    out.alpha_composite(down, ((out.width - down.width) // 2, top + icon.height - 2))
    return out


def cooldown_pair(button):
    """(ready, cooldown) of a finished deploy button, coloured as the game's timer buttons."""
    a = np.asarray(button, dtype=np.float64)
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    keep = (np.minimum(np.minimum(xx, yy), np.minimum(w - 1 - xx, h - 1 - yy)) >= BORDER) & (a[..., 3] > 0)
    ready = np.zeros_like(a)
    for c, (k, d) in enumerate(READY_FIT):
        ready[..., c] = k * a[..., c] + d
    ready[..., 3] = a[..., 3]
    grey = GREY[0] * (0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]) + GREY[1]
    cool = np.stack([grey, grey, grey, np.full_like(grey, GREY_ALPHA)], axis=-1)
    out = []
    for img in (ready, cool):
        img[~keep] = 0
        out.append(Image.fromarray(np.clip(np.rint(img), 0, 255).astype(np.uint8), 'RGBA'))
    return tuple(out)


def make(button_path, icon, prefix):
    """Write <prefix>_ready / _cooldown / _recall.dds for `icon` (an RGBA image) on the button at button_path."""
    button, header = load(button_path)
    ready, cool = cooldown_pair(with_icon(button, icon))
    save(f'{prefix}_ready.dds', ready, header)
    save(f'{prefix}_cooldown.dds', cool, header)
    save(f'{prefix}_recall.dds', recall(button, icon), header)


if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    make(sys.argv[1], load(sys.argv[2])[0], sys.argv[3])
