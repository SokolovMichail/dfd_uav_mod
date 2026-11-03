"""The Patchwork UAV's drone, rusty: its own model, material and textures, made from the swarm drone's (lgn_swarm) so
that the Legion's drones and the Recon Van's clean drone keep theirs.

  model     animations/new/skin/<name>.model - lgn_swarm.model with its material list (the MTRL section at the end:
            a count and null-terminated paths, then CLDR - no sizes before them) naming the new material; a humans.xml
            row's `mesh` names the model (the Legion's repair drone is lgn_swarm_repairdrone the same way)
  material  materials/vehicles/<name>.material - the swarm's, with the new albedo and rough textures and the
            emission dimmed (4.0 -> 2.2, as the game's own rusty Legion tank, lgn_tank_main_rust)
  textures  textures/vehicle/<name>_albedo.dds, <name>_rough.dds; the normal and emission maps stay the swarm's

The rust is made the way the game's rusty tank differs from the clean one: over rust patches (fractal noise, the
same every build) the albedo takes the colour and grain of the game's own tileable rust (textures/buildings/
rustmetal_albedo), and the rough texture - red: metalness, green: roughness - loses its metalness and gets rougher
(tank: 124 -> 14, 172 -> 220). Bright coloured markings (the red eye, warning paint) rust less. The textures are
written as uncompressed 32-bit DDS with every mipmap level (the game's are DXT1; it reads uncompressed ones as well).
"""
import json

import numpy as np
from PIL import Image

RUST = 0.85          # how much a fully rusty pixel takes of the rust
COVER = 0.55         # the share of the surface the patches cover (about)
MARKINGS = 0.35      # how much of the rust saturated paint gets
SEED = 1221
EMISSION_POWER = 2.2


def model_with_material(model, material):
    """The model's bytes with its material list replaced by one material path."""
    at = model.rfind(b'MTRL')
    end = model.rfind(b'CLDR')
    if at < 0 or end < at:
        raise SystemExit('the model has no MTRL ... CLDR at its end - check what the game changed')
    return model[:at] + b'MTRL' + (1).to_bytes(4, 'little') + material.encode() + b'\0' + model[end:]


def material(swarm_material, name):
    """The swarm's material (its JSON text) with the rusty textures and the emission dimmed."""
    m = json.loads(swarm_material)
    m['Textures']['albedo'] = f'textures/vehicle/{name}_albedo.dds'
    m['Textures']['rough'] = f'textures/vehicle/{name}_rough.dds'
    m.setdefault('Floats', {})['emission_power'] = EMISSION_POWER
    return json.dumps(m, indent=4) + '\n'


def noise(size, rng):
    """Fractal value noise in 0..1, size x size."""
    total = np.zeros((size, size))
    for cells, weight in ((4, 0.5), (8, 0.25), (16, 0.15), (48, 0.1)):
        grid = Image.fromarray((rng.random((cells, cells)) * 255).astype(np.uint8), 'L')
        total += np.asarray(grid.resize((size, size), Image.BICUBIC), dtype=np.float64) / 255 * weight
    return total


def rust_mask(size):
    """Where the rust is, 0..1: patches covering about COVER of the surface, soft at their edges."""
    n = noise(size, np.random.default_rng(SEED))
    edge = np.quantile(n, 1 - COVER)
    return np.clip((n - edge) / 0.08 + 0.5, 0, 1)


def rusty(albedo, rough, rust):
    """(albedo, rough) of the drone with rust: albedo and rough the swarm's (RGBA images of the same size), rust -
    the game's tileable rust albedo."""
    size = albedo.width
    a = np.asarray(albedo, dtype=np.float64)
    r = np.asarray(rough.resize(albedo.size, Image.BICUBIC), dtype=np.float64)
    tile = np.asarray(rust.convert('RGB').resize((size, size), Image.BICUBIC), dtype=np.float64)
    mask = rust_mask(size)
    rgb = a[..., :3]
    sat = (rgb.max(-1) - rgb.min(-1)) / np.maximum(rgb.max(-1), 1)
    paint = (sat > 0.45) & (rgb.max(-1) > 90)  # the red eye, warning paint
    m = (mask * np.where(paint, MARKINGS, 1.0) * RUST)[..., None]
    # the rust keeps the metal's light and shade: the tile's colour times the albedo's brightness against its own
    shade = (rgb @ [0.299, 0.587, 0.114] / max((rgb @ [0.299, 0.587, 0.114]).mean(), 1))[..., None]
    out_a = a.copy()
    out_a[..., :3] = rgb * (1 - m) + np.clip(tile * (0.6 + 0.4 * shade), 0, 255) * m
    out_r = r.copy()
    m2 = m[..., 0]
    out_r[..., 0] = r[..., 0] * (1 - m2) + 14 * m2     # metalness -> almost none
    out_r[..., 1] = r[..., 1] * (1 - m2) + 220 * m2    # roughness -> rough
    to = lambda x: Image.fromarray(np.clip(np.rint(x), 0, 255).astype(np.uint8), 'RGBA')  # noqa: E731
    return to(out_a), to(out_r)


def write_dds(path, image):
    """An uncompressed 32-bit DDS (R in the low byte, as the game's own) with every mipmap level."""
    w, h = image.size
    levels, data, level = 0, b'', image
    while True:
        data += level.tobytes()
        levels += 1
        if level.width == 1 and level.height == 1:
            break
        level = level.resize((max(1, level.width // 2), max(1, level.height // 2)), Image.LANCZOS)
    u32 = lambda v: int(v).to_bytes(4, 'little')  # noqa: E731
    flags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000  # caps, height, width, pitch, pixel format, mipmap count
    pixel_format = (u32(32) + u32(0x41) + u32(0) + u32(32)  # size, RGB + alpha, no fourcc, 32 bits
                    + u32(0x000000FF) + u32(0x0000FF00) + u32(0x00FF0000) + u32(0xFF000000))
    caps = u32(0x1000 | 0x400000 | 0x8)  # texture, mipmap, complex
    header = (b'DDS ' + u32(124) + u32(flags) + u32(h) + u32(w) + u32(w * 4) + u32(0) + u32(levels)
              + bytes(44) + pixel_format + caps + bytes(16))
    assert len(header) == 128
    with open(path, 'wb') as f:
        f.write(header + data)
    print(f'{path}: {w}x{h}, {levels} levels')
