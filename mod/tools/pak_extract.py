"""Распаковка файлов из .pak игры (zip с паролем ZipCrypto).

    python mod/tools/pak_extract.py <архив.pak> [маска ...] [-o папка]   # без маски - список файлов

Пароль: DFD_PAK_PASSWORD - переменная окружения или строка в .env в корне проекта (в git не кладётся).
Маски - fnmatch по пути внутри архива (например locale/cars.xml или 'scripts/species/*').
По умолчанию -o mod/extracted/<имя архива>.
"""
import fnmatch
import os
import sys
import zipfile

here = os.path.dirname(os.path.abspath(__file__))
args = sys.argv[1:]
out = None
if '-o' in args:
    k = args.index('-o')
    out = args[k + 1]
    del args[k:k + 2]
if not args:
    sys.exit(__doc__)
pak, masks = args[0], args[1:]
sys.path.insert(0, here)
from gamefiles import password  # noqa: E402

pw = password()
z = zipfile.ZipFile(pak)
if not masks:
    for i in z.infolist():
        print(f'{i.file_size:>10}  {i.filename}')
    sys.exit()
out = out or os.path.join(here, '..', 'extracted', os.path.splitext(os.path.basename(pak))[0])
for i in z.infolist():
    if i.is_dir() or not any(fnmatch.fnmatch(i.filename, m) for m in masks):
        continue
    dst = os.path.join(out, i.filename)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, 'wb') as f:
        f.write(z.read(i, pwd=pw))
    print(dst)
