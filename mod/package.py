"""Packs a release: an archive laid out like the game folder. Four of them - the framework alone or with UAV
Carriers, standalone or for DefianceLoader:

    python mod/package.py [--mod <built UAV_Carriers>] --framework-mod <built Spawn_Framework>
                          --winhttp <winhttp.dll> --plugin <SpawnFramework.dll> --version 1.0 --out <dist>
    python mod/package.py --defiance [--mod <...>] --framework-mod <...> --plugin <spawn_framework.dll>
                          --manifest <spawn_framework.plugin.json> --version 1.0 --out <dist>

- without --mod - the framework only: SpawnFramework_v<version>.zip (with --defiance -
  SpawnFramework_DefianceLoader_v<version>.zip): the plugin + mods/Spawn_Framework;
- with --mod - the framework + UAV Carriers (mods/UAV_Carriers): UAV_Carriers_v<version>.zip
  (UAV_Carriers_DefianceLoader_v<version>.zip); with --cover-from <game files, layer base> also the mod page's
  cover <out>/UAV_Carriers_v<version>_cover.png (1920x1080) and banner _banner.png (1300x372).

The players' README is docs/Dist-readme.md: blocks {IF STANDALONE} / {IF DEFIANCE} and {IF FRAMEWORK} /
{IF EXAMPLE} ... {END} go only into their own package - on lines of their own (they may nest), or inside a line
(not nested; they may run on over line breaks). A step written "*." is numbered after the step before it.
{VERSION} is the version. The modders' guide docs/MODDING.md goes into every package.

The archive is reproducible: entries in a fixed order, with a fixed date and attributes, so the same files give a
byte-identical zip (with the same zlib - the one bundled with the Python used). The sha256 of the archive and of the
plugin files in it go to <out>/SHA256SUMS.txt (lines of other packages already there are kept). As HEMTT Spares'
(DFDRepairMod mod/package.py).
"""
import argparse
import hashlib
import os
import re
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMEWORK, FRAMEWORK_FOLDER = 'SpawnFramework', 'Spawn_Framework'
EXAMPLE = 'UAV_Carriers'
ZIP_DATE = (1980, 1, 1, 0, 0, 0)  # the earliest date a zip can hold; the same in every build
SUMS = 'SHA256SUMS.txt'
# The art the cover and banner are drawn from (mod/uav_carriers/build_mod.py ICON_PICTURES), under the game files.
COVER_PICTURES = ['textures/ui/pictures/tech_pic/vehicles_icons_big/int_guntruck.dds',
                  'textures/ui/pictures/tech_pic/vehicles_icons_big/Res_van.dds',
                  'textures/ui/pictures/tech_pic/infantry_icons_big/lgn_drones.dds']


def zip_entry(arc):
    """A zip entry with no build-dependent fields: fixed date, a regular rw-r--r-- file, made on "MS-DOS"."""
    info = zipfile.ZipInfo(arc, date_time=ZIP_DATE)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 0
    info.external_attr = 0o100644 << 16
    return info


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def update_sums(path, sums):
    """Write sums ({name: sha256}) into the SHA256SUMS file path, keeping the lines of other names; sorted by name."""
    old = {}
    if os.path.isfile(path):
        for line in open(path, encoding='utf-8'):
            h, _, name = line.rstrip('\n').partition('  ')
            if name:
                old[name] = h
    old.update(sums)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        for name in sorted(old):
            f.write(f'{old[name]}  {name}\n')


def pick_variant(text, tags):
    """Keep the README's blocks whose tag is in tags, drop the rest (see the module's docstring), number the "*."
    steps."""
    # inside a line: {IF X}...{END}, the opening not at the end of its line
    text = re.sub(r'\{IF (\w+)\}(?!\n)(.*?)\{END\}', lambda m: m.group(2) if m.group(1) in tags else '', text,
                  flags=re.S)
    out, stack = [], []
    for line in text.splitlines(keepends=True):
        tag = line.strip()
        if tag.startswith('{IF ') and tag.endswith('}'):
            stack.append(tag[4:-1] in tags)
        elif tag == '{END}':
            if not stack:
                sys.exit('README: {END} without {IF}')
            stack.pop()
        elif all(stack):
            out.append(line)
    if stack:
        sys.exit('README: {IF} without {END}')
    numbered, last = [], 0
    for line in out:
        m = re.match(r'(\d+)\. ', line)
        if m:
            last = int(m.group(1))
        elif line.startswith('*. '):
            last += 1
            line = f'{last}. ' + line[3:]
        elif line.startswith('#'):
            last = 0
        numbered.append(line)
    return ''.join(numbered)


def cover_and_banner(game_src, out, version):
    """UAV Carriers' cover (the MODS icon, 1920x1080) and banner (1300x372) for the mod page."""
    sys.path[:0] = [os.path.join(HERE, 'uav_carriers', 'art'), os.path.join(HERE, 'framework', 'art')]
    import mod_icon  # noqa: E402
    pictures = [os.path.join(game_src, *rel.split('/')) for rel in COVER_PICTURES]
    missing = [p for p in pictures if not os.path.isfile(p)]
    if missing:
        sys.exit('no game pictures for the cover: ' + ', '.join(missing))
    cover = os.path.join(out, f'{EXAMPLE}_v{version}_cover.png')
    mod_icon.render(*pictures).convert('RGB').save(cover)
    banner = os.path.join(out, f'{EXAMPLE}_v{version}_banner.png')
    mod_icon.render_banner(*pictures).convert('RGB').save(banner)
    print(f'-> {cover}\n-> {banner}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--defiance', action='store_true', help='pack the DefianceLoader plugin, not the standalone one')
    ap.add_argument('--mod', help='the built UAV_Carriers; without it - the framework only')
    ap.add_argument('--framework-mod', required=True, help='the built Spawn_Framework')
    ap.add_argument('--winhttp', help='the standalone build only')
    ap.add_argument('--plugin', required=True)
    ap.add_argument('--manifest', help='--defiance only: spawn_framework.plugin.json')
    ap.add_argument('--cover-from', help='with --mod: the game files (layer base) to draw the cover and banner from')
    ap.add_argument('--version', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    if a.defiance and not a.manifest:
        ap.error('--defiance needs --manifest')
    if not a.defiance and not a.winhttp:
        ap.error('needs --winhttp (or --defiance)')

    files = []  # (path on disk, path in the archive)

    def add_mod(folder, name):
        """Every file of the built mod folder into the archive as mods/<name>/..."""
        folder = os.path.abspath(folder)
        for dirpath, dirs, names in os.walk(folder):
            dirs.sort()  # a fixed entry order, whatever order the file system lists folders in
            for n in sorted(names):
                p = os.path.join(dirpath, n)
                files.append((p, f'mods/{name}/' + os.path.relpath(p, folder).replace('\\', '/')))

    add_mod(a.framework_mod, FRAMEWORK_FOLDER)
    if a.mod:
        add_mod(a.mod, EXAMPLE)
    if a.defiance:  # DefianceLoader takes the plugin together with its manifest from DefianceLoader\plugins
        files += [(a.plugin, 'DefianceLoader/plugins/spawn_framework.dll'),
                  (a.manifest, 'DefianceLoader/plugins/spawn_framework.plugin.json')]
    else:
        files += [(a.winhttp, 'bin/winhttp.dll'),
                  (a.plugin, 'bin/plugins/SpawnFramework.dll')]
    base = EXAMPLE if a.mod else FRAMEWORK
    files.append((os.path.join(HERE, '..', 'docs', 'Dist-readme.md'), f'{base}_README.txt'))
    files.append((os.path.join(HERE, '..', 'docs', 'MODDING.md'), f'{FRAMEWORK}_MODDING.txt'))
    missing = [p for p, _ in files if not os.path.isfile(p)]
    if missing:
        sys.exit('missing files: ' + ', '.join(missing))
    if a.mod and any('Test_troop_van' in open(p, encoding='utf-8', errors='ignore').read()
                     for p, arc in files if arc.endswith('cars.xml')):
        sys.exit('the mod was built with the test vehicles (SPAWN_TEST_VEHICLES): not for a release')

    tags = {'DEFIANCE' if a.defiance else 'STANDALONE', 'EXAMPLE' if a.mod else 'FRAMEWORK'}
    os.makedirs(a.out, exist_ok=True)
    zip_path = os.path.join(a.out, f'{base}{"_DefianceLoader" if a.defiance else ""}_v{a.version}.zip')
    sums = {}
    with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p, arc in files:
            if arc.endswith('README.txt'):  # the README's variant and version are picked at packing
                text = pick_variant(open(p, encoding='utf-8').read(), tags)
                data = text.replace('{VERSION}', a.version).replace('\n', '\r\n').encode('utf-8')
            elif arc.endswith('MODDING.txt'):
                data = open(p, encoding='utf-8').read().replace('\n', '\r\n').encode('utf-8')
            else:
                data = open(p, 'rb').read()
            z.writestr(zip_entry(arc), data)
            if arc.endswith(('.dll', '.plugin.json')):  # the plugin itself: players compare these with SHA256SUMS
                sums[arc] = sha256(data)
            print(f'{len(data):>10}  {arc}')
    sums[os.path.basename(zip_path)] = sha256(open(zip_path, 'rb').read())
    update_sums(os.path.join(a.out, SUMS), sums)
    print(f'-> {zip_path} ({os.path.getsize(zip_path)} bytes), sha256 {sums[os.path.basename(zip_path)]}')
    if a.mod and a.cover_from:
        cover_and_banner(a.cover_from, a.out, a.version)


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    main()
