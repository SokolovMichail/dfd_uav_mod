"""UAV Carriers' MODS menu icon (also the mod page's cover) and the page's banner, in the framework's style
(framework/art/mod_art.py): the two carriers - the game's own pictures of the vehicles they are built on - each
with its drone hovering above it on a dashed tether; the Patchwork one's drone rusty, its eye glowing green.

    python mod/uav_carriers/art/mod_icon.py <int_guntruck.dds> <Res_van.dds> <lgn_drones.dds> <out.dds> [cover.png]
    python mod/uav_carriers/art/mod_icon.py --banner <int_guntruck.dds> <Res_van.dds> <lgn_drones.dds> <banner.png>
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path[:0] = [HERE, os.path.join(HERE, '..', '..', 'framework', 'art')]
import buttons  # noqa: E402
import mod_art as art  # noqa: E402
from front_drone import front_drone  # noqa: E402

RUST = (1.25, 0.72, 0.45)       # the Patchwork drone: its light blue multiplied towards rust
EYE = (90, 255, 80, 255)        # and its eye, as its model's (rusty_drone.GREEN)


def rusty(drone):
    """The drone icon tinted rust, with a green eye glowing at its nose."""
    a = np.asarray(drone, dtype=np.float64).copy()
    for c, k in enumerate(RUST):
        a[..., c] *= k
    out = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8), 'RGBA')
    r = max(2, out.width // 14)
    cx, cy = out.width // 2, round(out.height * 0.16)
    glow = Image.new('RGBA', out.size, (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse([cx - 3 * r, cy - 3 * r, cx + 3 * r, cy + 3 * r], fill=EYE[:3] + (110,))
    out.alpha_composite(glow.filter(ImageFilter.GaussianBlur(r * 1.5)))
    ImageDraw.Draw(out).ellipse([cx - r, cy - r, cx + r, cy + r], fill=EYE)
    return out


def tether(img, a, b, width, dash):
    """A dashed line from a to b: the drone's link to its carrier."""
    d = ImageDraw.Draw(img)
    length = math.dist(a, b)
    n = int(length // (2 * dash))
    for k in range(n + 1):
        t0, t1 = 2 * k * dash / length, min(1, (2 * k + 1) * dash / length)
        p0 = (a[0] + (b[0] - a[0]) * t0, a[1] + (b[1] - a[1]) * t0)
        p1 = (a[0] + (b[0] - a[0]) * t1, a[1] + (b[1] - a[1]) * t1)
        d.line([p0, p1], fill=(214, 166, 98, 170), width=width)


def carrier(img, vehicle, drone, cx, base_y, width, drone_height):
    """A carrier at cx (its wheels on base_y) and its drone above and behind it, on a dashed tether."""
    v = art.scaled(vehicle, width=width)
    x, y = cx - v.width // 2, base_y - v.height
    dr = art.scaled(drone, height=drone_height)
    dx, dy = cx + v.width // 5, y - round(drone_height * 1.6)
    tether(img, (cx, y + v.height // 6), (dx, dy + dr.height // 2), max(2, width // 160), max(6, width // 40))
    art.with_shadow(img, v, x, y, blur=max(4, width // 55), offset=(width // 55, width // 45))
    img.alpha_composite(dr, (dx - dr.width // 2, dy))  # it flies: no shadow under it


def pictures(guntruck, van, drones):
    """(the two vehicles, the clean drone, the rusty drone) from the game's pictures."""
    drone = np.asarray(front_drone(buttons.load(drones)[0])).copy()
    # its fill only: not the icon's faint haze (large, its shadow would be a box) nor its dark outline, left ragged
    # at the tail where it was cut out of the three drones - on the dark background the outline adds nothing
    drone[(drone[..., 3] < 60) | (drone[..., 0] < 60)] = 0
    drone = Image.fromarray(drone, 'RGBA')
    return buttons.load(guntruck)[0], buttons.load(van)[0], drone, rusty(drone)


def render(guntruck, van, drones):
    """The icon / cover, 1920x1080: the name, the two carriers with their drones, their names."""
    truck, van_pic, clean, rust = pictures(guntruck, van, drones)
    img = art.background()
    art.centered(img, art.W / 2, 70, 'UAV CARRIERS', art.font(140, 'Bold'), art.CREAM, glow=True)
    art.centered(img, art.W / 2, 245, 'A RECON DRONE ON EVERY DEPLOY', art.font(56, 'SemiBold'), art.AMBER)
    base = 900
    for cx, vehicle, drone, label in ((500, truck, clean, 'UAV CARRIER'),
                                      (1420, van_pic, rust, 'PATCHWORK UAV CARRIER')):
        carrier(img, vehicle, drone, cx, base, 640, 170)
        art.centered(img, cx, base + 40, label, art.font(54, 'SemiBold'), art.CREAM)
    return img


def render_banner(guntruck, van, drones):
    """The mod page's banner, 1300x372: the name on the left, the two carriers with their drones on the right."""
    truck, van_pic, clean, rust = pictures(guntruck, van, drones)
    bw, bh = art.BANNER
    img = art.background(bw, bh)
    art.centered(img, 320, 70, 'UAV', art.font(84, 'Bold'), art.CREAM, glow=True)
    art.centered(img, 320, 160, 'CARRIERS', art.font(84, 'Bold'), art.CREAM, glow=True)
    art.centered(img, 320, 270, 'A RECON DRONE ON EVERY DEPLOY', art.font(26, 'SemiBold'), art.AMBER)
    for cx, vehicle, drone in ((790, truck, clean), (1110, van_pic, rust)):
        carrier(img, vehicle, drone, cx, 330, 250, 66)
    return img


if __name__ == '__main__':
    args = sys.argv[1:]
    if args[:1] == ['--banner']:
        if len(args) != 5:
            sys.exit(__doc__)
        render_banner(*args[1:4]).convert('RGB').save(args[4])
        print(f'{args[4]}: {art.BANNER[0]}x{art.BANNER[1]}')
        sys.exit()
    if len(args) < 4:
        sys.exit(__doc__)
    icon = render(*args[:3])
    art.save_dds(icon, args[3])
    if len(args) > 4:
        icon.convert('RGB').save(args[4])
