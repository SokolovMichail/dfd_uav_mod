"""The Spawn Framework's MODS menu icon, and the drawing the mods made on it share for theirs - in the style of
HEMTT Spares' (DFDRepairMod mod/art/make_mod_icon.py): a dark gradient with the campaign map's diagonal hatching
and a vignette, Bahnschrift titles in cream and amber, the game's own pictures large with a soft shadow.

    python mod/framework/art/mod_art.py <buttons_unpack_n.dds> <rifle squad icon.dds> <out.dds> [preview.png]

The framework's icon shows what it does, since it adds no vehicles: F4's three pictures (buttons.py, from the
generic rifle squad's icon) - deploy, the cooldown filling up, recall - large and labelled.

A MODS icon is 1920x1080, uncompressed 32-bit DDS with every mipmap level, as the game's mods/mod_example_1.
"""
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import buttons  # noqa: E402

W, H = 1920, 1080
BANNER = (1300, 372)  # the mod page's preview
FONT = os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'Fonts', 'bahnschrift.ttf')
CREAM, AMBER = (236, 230, 214, 255), (214, 166, 98, 255)


def background(w=W, h=H):
    """w x h: a vertical gradient, diagonal hatching, a vignette (its size in proportion to the height)."""
    img = Image.new('RGBA', (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / h
        c = tuple(int(a + (b - a) * t) for a, b in zip((30, 40, 46), (12, 16, 19)))
        d.line([(0, y), (w, y)], fill=c + (255,))
    hatch = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    hd = ImageDraw.Draw(hatch)
    for x in range(-h, w, 28):
        hd.line([(x, h), (x + h, 0)], fill=(255, 255, 255, 10), width=6)
    img.alpha_composite(hatch)
    vig = Image.new('L', (w, h), 0)
    ImageDraw.Draw(vig).ellipse([-300 * h // H, -250 * h // H, w + 300 * h // H, h + 250 * h // H], fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(160 * h / H))
    dark = Image.new('RGBA', (w, h), (0, 0, 0, 255))
    dark.putalpha(vig.point(lambda v: 255 - v))
    img.alpha_composite(dark)
    return img


def font(size, weight):
    """Bahnschrift at `size`, of a weight (Bold, SemiBold...)."""
    f = ImageFont.truetype(FONT, size)
    f.set_variation_by_name(weight)
    return f


def centered(img, x, y, s, f, fill, glow=False):
    """Text s centred on x, its top at y; glow - a blurred dark backing for legibility."""
    d = ImageDraw.Draw(img)
    tw = d.textlength(s, font=f)
    if glow:
        g = Image.new('RGBA', img.size, (0, 0, 0, 0))
        ImageDraw.Draw(g).text((x - tw / 2, y), s, font=f, fill=(0, 0, 0, 200))
        img.alpha_composite(g.filter(ImageFilter.GaussianBlur(12)))
    d.text((x - tw / 2, y), s, font=f, fill=fill)


def scaled(pic, width=None, height=None):
    """The picture cropped to what it draws and scaled to a width or a height (LANCZOS)."""
    pic = pic.crop(pic.getbbox())
    k = width / pic.width if width else height / pic.height
    return pic.resize((max(1, round(pic.width * k)), max(1, round(pic.height * k))), Image.LANCZOS)


def with_shadow(img, pic, x, y, blur=12, offset=(12, 16)):
    """Paste pic at (x, y) over a soft shadow of itself."""
    sh = Image.new('RGBA', pic.size, (0, 0, 0, 0))
    sh.putalpha(pic.getchannel('A').point(lambda v: int(v * 0.6)))
    sh = sh.filter(ImageFilter.GaussianBlur(blur))
    img.alpha_composite(sh, (x + offset[0], y + offset[1]))
    img.alpha_composite(pic, (x, y))


def filling(ready, cooldown, part):
    """The deploy button with the cooldown's grey over its top `part` - as F4's bar draws the wait still to go."""
    out = ready.copy()
    cut = round(ready.height * part)
    out.alpha_composite(cooldown.crop((0, 0, ready.width, cut)), (0, 0))
    return out


def render_framework(button, icon):
    """The framework's icon: its name, and F4's three pictures made from `icon` on `button` (RGBA), labelled."""
    img = background()
    centered(img, W / 2, 70, 'SPAWN FRAMEWORK', font(140, 'Bold'), CREAM, glow=True)
    centered(img, W / 2, 245, 'UNITS ON DEPLOY  +  F4 COOLDOWN', font(56, 'SemiBold'), AMBER)
    ready, cooldown = buttons.cooldown_pair(buttons.with_icon(button, icon))
    recall = buttons.recall(button, icon)
    size = 300
    up = lambda p: p.resize((size, size), Image.NEAREST)  # noqa: E731  the game's pixels, crisp
    pictures = [(460, up(ready), 'DEPLOY'), (960, up(filling(ready, cooldown, 0.55)), 'COOLDOWN'),
                (1460, up(recall), 'RECALL')]
    top = 470
    for cx, pic, label in pictures:
        with_shadow(img, pic, cx - size // 2, top)
        centered(img, cx, top + size + 50, label, font(54, 'SemiBold'), CREAM)
    return img


def save_dds(img, path):
    """A8R8G8B8, uncompressed, every mipmap level - as mods/mod_example_1/basis/mod_icon.dds."""
    levels, cur = [img], img
    while cur.size != (1, 1):
        cur = cur.resize((max(1, cur.width // 2), max(1, cur.height // 2)), Image.LANCZOS)
        levels.append(cur)
    hdr = bytearray(128)
    hdr[0:4] = b'DDS '
    # size, flags (CAPS|HEIGHT|WIDTH|PITCH|PIXELFORMAT|MIPMAPCOUNT), h, w, pitch, depth, mipmaps
    struct.pack_into('<7I', hdr, 4, 124, 0x2100F, img.height, img.width, img.width * 4, 0, len(levels))
    # pixel format: size, flags (ALPHAPIXELS|RGB), fourcc, bits, masks R G B A
    struct.pack_into('<8I', hdr, 76, 32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    struct.pack_into('<I', hdr, 108, 0x401008)  # COMPLEX | MIPMAP | TEXTURE
    with open(path, 'wb') as f:
        f.write(hdr)
        for lv in levels:
            r, g, b, a = lv.split()
            f.write(Image.merge('RGBA', (b, g, r, a)).tobytes())
    print(f'{path}: {img.width}x{img.height}, {len(levels)} levels')


if __name__ == '__main__':
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    icon_img = render_framework(buttons.load(sys.argv[1])[0], buttons.load(sys.argv[2])[0])
    save_dds(icon_img, sys.argv[3])
    if len(sys.argv) > 4:
        icon_img.convert('RGB').save(sys.argv[4])
