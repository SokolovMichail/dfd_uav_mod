"""Takes the current versions of files from the installed game.

The game mounts layers in order (each later file replaces the earlier one whole):
    <layer>/basis.pak -> <layer>/patch_NNN*.pak by number -> <layer>/basis/ (unpacked files)
So the current version of a file is the one from the last source that has it.

    from gamefiles import Game
    g = Game(r'C:\\Games\\Terminator Dark Fate - Defiance')
    g.extract('base', 'scripts/species/cars.xml', 'cache/base')   # -> the file's path + where it came from
"""
import glob
import os
import re
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))

# layer -> (folder relative to the game, the archives' mask, the folder of unpacked files or None)
LAYERS = {
    'base': ('.', '*.pak', 'basis'),
    'resistance': ('dlc/Resistance', '*.pak', 'basis'),
    'loc_en': ('localization/en', '*.pak', None),
    'resistance_loc_en': ('dlc/Resistance/localization/en', '*.pak', None),
    'canada': ('dlc/Canada', '*.pak', 'basis'),
    'canada_loc_en': ('dlc/Canada/localization/en', '*.pak', None),
}


def env(name):
    """An environment variable, or a NAME=value line from .env in the project's root (no $ substitution)."""
    if os.environ.get(name):
        return os.environ[name]
    path = os.path.join(HERE, '..', '..', '.env')
    if os.path.isfile(path):
        for line in open(path, encoding='utf-8'):
            key, sep, value = line.strip().partition('=')
            if sep and key.strip() == name and not key.lstrip().startswith('#'):
                value = value.strip()
                if len(value) >= 2 and value[0] == value[-1] and value[0] in '"\'':
                    value = value[1:-1]
                return value
    return None


def password():
    """The game archives' password (bytes) from DFD_PAK_PASSWORD; without it - exit with a hint."""
    pw = env('DFD_PAK_PASSWORD')
    if not pw:
        raise SystemExit('no password for the game archives: set DFD_PAK_PASSWORD (an environment variable or .env in the project root)')
    return pw.encode()


def _order(path):
    """Sort key for a layer's archives in mount order: basis.pak, then patch_NNN by number, the rest at the end."""
    name = os.path.basename(path).lower()
    if name.startswith('basis'):
        return -1
    m = re.match(r'patch_(\d+)', name)
    return int(m.group(1)) if m else 10 ** 6


class Game:
    def __init__(self, root):
        if not os.path.isfile(os.path.join(root, 'bin', 'logic.dll')):
            raise SystemExit(f'does not look like the game folder: {root}')
        self.root = root
        self._pw = None
        self._index = {}

    def _sources(self, layer):
        """[(archive, {lower-case name: name})] in mount order."""
        if layer not in self._index:
            folder, mask, _ = LAYERS[layer]
            paks = sorted(glob.glob(os.path.join(self.root, folder, mask)), key=_order)
            self._index[layer] = [(p, {i.filename.lower(): i.filename for i in zipfile.ZipFile(p).infolist()})
                                  for p in paks]
        return self._index[layer]

    def locate(self, layer, rel):
        """Where to take the file from: ('loose', path) or ('pak', archive, name inside)."""
        folder, _, loose = LAYERS[layer]
        if loose:
            p = os.path.join(self.root, folder, loose, rel)
            if os.path.isfile(p):
                return ('loose', p)
        found = None
        for pak, names in self._sources(layer):
            if rel.lower() in names:
                found = ('pak', pak, names[rel.lower()])
        if not found:
            raise SystemExit(f'layer {layer} has no file {rel}')
        return found

    def extract(self, layer, rel, out_root):
        """Copies the current version to out_root/rel. Returns (path, description of the source)."""
        src = self.locate(layer, rel)
        dst = os.path.join(out_root, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        if src[0] == 'loose':
            data = open(src[1], 'rb').read()
            origin = os.path.relpath(src[1], self.root)
        else:
            if self._pw is None:
                self._pw = password()
            data = zipfile.ZipFile(src[1]).read(src[2], pwd=self._pw)
            origin = os.path.relpath(src[1], self.root)
        with open(dst, 'wb') as f:
            f.write(data)
        return dst, origin
