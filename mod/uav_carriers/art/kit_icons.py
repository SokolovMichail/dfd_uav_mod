"""The UAV kit's ammunition pictures, in the style of the game's own (textures/ui/pictures/Ammunition): a flat
silhouette of the drone, no outline, coloured like the smoke round's picture in each of its six states - <name>,
_o (hover), _p (pressed), _off, _off_o, _off_p (not available: dimmer, with the game's red cross over it).

For each state the smoke round's picture gives the colours: its icon's darkest and lightest teal (the 5th and 95th
percentile of its opaque pixels, the red cross left out), and the drone's own shading is mapped between them; the
_off states also take the smoke picture's red cross. The game's pictures of the three bright states are BC7; the
outputs are uncompressed 32-bit DDS (as the game's _off ones are) with the header of the smoke round's _off picture
(same size, 88x52). The game's files are only read.
"""
import numpy as np
from PIL import Image

STATES = ['', '_o', '_p', '_off', '_off_o', '_off_p']
HEIGHT = 40  # the drone's height in the 88x52 picture


def red_cross(img):
    """The red cross of an _off picture alone (its reddish pixels)."""
    a = np.asarray(img, dtype=np.int32)
    red = (a[..., 0] > a[..., 1] + 40) & (a[..., 0] > a[..., 2] + 40)
    out = np.zeros_like(a)
    out[red] = a[red]
    return Image.fromarray(out.astype(np.uint8), 'RGBA')


def palette(img):
    """(dark, light, opacity) of the icon in a picture: its teal pixels' colours (not the cross nor its reddish
    shadow) and their usual opacity - the _off states' icons are half transparent."""
    a = np.asarray(img, dtype=np.float64)
    keep = (a[..., 3] > 100) & (a[..., 1] >= a[..., 0])
    px = a[keep][:, :3]
    lum = px @ [0.299, 0.587, 0.114]
    lo, hi = np.percentile(lum, 5), np.percentile(lum, 95)
    return px[lum <= lo].mean(0), px[lum >= hi].mean(0), np.median(a[keep][:, 3]) / 255


def silhouette(drone, size, dark, light, opacity):
    """The drone flat-coloured: its shading mapped between dark and light, in a picture of `size`. Its dark outline
    goes (the game's ammunition icons have none): only its fill is kept."""
    a = np.asarray(drone).copy()
    a[(a[..., 0] < 60) | (a[..., 3] < 100)] = 0
    drone = Image.fromarray(a, 'RGBA')
    d = drone.resize((max(1, round(drone.width * HEIGHT / drone.height)), HEIGHT), Image.LANCZOS)
    a = np.asarray(d, dtype=np.float64)
    lum = a[..., :3] @ [0.299, 0.587, 0.114]
    inside = a[..., 3] > 0
    lo, hi = np.percentile(lum[inside], 5), np.percentile(lum[inside], 95)
    t = np.clip((lum - lo) / max(hi - lo, 1), 0, 1)[..., None]
    rgb = dark + (light - dark) * t
    out = np.dstack([rgb, a[..., 3] * opacity])
    icon = Image.fromarray(np.clip(np.rint(out), 0, 255).astype(np.uint8), 'RGBA')
    pic = Image.new('RGBA', size, (0, 0, 0, 0))
    pic.alpha_composite(icon, ((size[0] - icon.width) // 2, (size[1] - icon.height) // 2))
    return pic


def make(drone, smoke_pictures, header, prefix):
    """Write <prefix><state>.dds for every state. drone - the drone icon (RGBA); smoke_pictures - state -> the
    smoke round's picture (RGBA); header - a 128-byte header of an uncompressed 32-bit 88x52 DDS."""
    for state in STATES:
        smoke = smoke_pictures[state]
        pic = silhouette(drone, smoke.size, *palette(smoke))
        if state.startswith('_off'):
            pic.alpha_composite(red_cross(smoke))
        with open(f'{prefix}{state}.dds', 'wb') as f:
            f.write(header + pic.tobytes())
        print(f'{prefix}{state}.dds: {pic.width}x{pic.height}')
