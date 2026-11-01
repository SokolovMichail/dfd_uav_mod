"""Достаёт актуальные версии файлов из установленной игры.

Игра монтирует слои по порядку (каждый следующий файл заменяет предыдущий целиком):
    <слой>/basis.pak -> <слой>/patch_NNN*.pak по номеру -> <слой>/basis/ (распакованные файлы)
Поэтому актуальная версия файла - из последнего источника, где он есть.

    from gamefiles import Game
    g = Game(r'C:\\Games\\Terminator Dark Fate - Defiance')
    g.extract('base', 'scripts/species/cars.xml', 'cache/base')   # -> путь к файлу + откуда он взят
"""
import glob
import os
import re
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))

# слой -> (папка относительно игры, маска архивов, папка распакованных файлов или None)
LAYERS = {
    'base': ('.', '*.pak', 'basis'),
    'resistance': ('dlc/Resistance', '*.pak', 'basis'),
    'loc_en': ('localization/en', '*.pak', None),
    'resistance_loc_en': ('dlc/Resistance/localization/en', '*.pak', None),
    'canada': ('dlc/Canada', '*.pak', 'basis'),
    'canada_loc_en': ('dlc/Canada/localization/en', '*.pak', None),
}


def env(name):
    """Переменная окружения или строка NAME=значение из .env в корне проекта (без подстановок $)."""
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
    """Пароль архивов игры (bytes) из DFD_PAK_PASSWORD; без него - выход с подсказкой."""
    pw = env('DFD_PAK_PASSWORD')
    if not pw:
        raise SystemExit('нет пароля архивов игры: задайте DFD_PAK_PASSWORD (переменная окружения или .env в корне проекта)')
    return pw.encode()


def _order(path):
    """Ключ сортировки архивов слоя в порядке монтирования: basis.pak, затем patch_NNN по номеру, прочие - в конце."""
    name = os.path.basename(path).lower()
    if name.startswith('basis'):
        return -1
    m = re.match(r'patch_(\d+)', name)
    return int(m.group(1)) if m else 10 ** 6


class Game:
    def __init__(self, root):
        if not os.path.isfile(os.path.join(root, 'bin', 'logic.dll')):
            raise SystemExit(f'не похоже на папку игры: {root}')
        self.root = root
        self._pw = None
        self._index = {}

    def _sources(self, layer):
        """[(архив, {имя в нижнем регистре: имя})] в порядке монтирования."""
        if layer not in self._index:
            folder, mask, _ = LAYERS[layer]
            paks = sorted(glob.glob(os.path.join(self.root, folder, mask)), key=_order)
            self._index[layer] = [(p, {i.filename.lower(): i.filename for i in zipfile.ZipFile(p).infolist()})
                                  for p in paks]
        return self._index[layer]

    def locate(self, layer, rel):
        """Откуда брать файл: ('loose', путь) или ('pak', архив, имя внутри)."""
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
            raise SystemExit(f'в слое {layer} нет файла {rel}')
        return found

    def extract(self, layer, rel, out_root):
        """Копирует актуальную версию в out_root/rel. Возвращает (путь, описание источника)."""
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
