"""Package the Vita executable, launcher art and dependency notices."""
import argparse
from contextlib import nullcontext
import hashlib
import json
from pathlib import Path
import struct
from zipfile import ZIP_DEFLATED, ZipFile

from generate_public_art import prepare

ROOT = Path(__file__).resolve().parents[1]
VERSION = '1.0.5'
APP_VERSION = '00.27'
APP_TITLE = "Five Nights At Freddy's"
NAME = f'FNAF1-Vita-v{VERSION}.vpk'
LIVEAREA = 'sce_sys/livearea/contents/'
LAUNCHER_ART = {
    'sce_sys/icon0.png': 'icon0.png',
    LIVEAREA + 'background.png': 'background.png',
    LIVEAREA + 'startup.png': 'startup.png',
}
LAUNCHER_FILES = frozenset((*LAUNCHER_ART, LIVEAREA + 'template.xml'))
REQUIRED_FILES = LAUNCHER_FILES | {'eboot.bin', 'sce_sys/param.sfo'}
DEFAULT_TEMPLATE = (
    '<?xml version="1.0" encoding="utf-8"?>'
    '<livearea style="a1" format-ver="01.00" content-rev="1">'
    '<livearea-background><image>background.png</image></livearea-background>'
    '<gate><startup-image>startup.png</startup-image></gate></livearea>'
)


def verify(path):
    path = Path(path)
    with ZipFile(path) as package:
        members = package.namelist()
        names = set(members)
        if package.testzip():
            raise ValueError('Archive CRC failure')
        if len(names) != len(members) or not REQUIRED_FILES <= names:
            raise ValueError('Missing or duplicated VPK member')
        for name in names - REQUIRED_FILES:
            parts = name.split('/')
            if len(parts) != 2 or parts[0] != 'licenses' or not parts[1].endswith('.txt'):
                raise ValueError(f'Unexpected VPK member {name}')
        if package.read('eboot.bin')[:4] != b'SCE\0':
            raise ValueError('Not a Vita SELF')

        raw = package.read('sce_sys/param.sfo')
        magic, _, keys, values, count = struct.unpack_from('<5I', raw)
        if magic != 0x46535000:
            raise ValueError('Invalid PARAM.SFO header')
        fields = {}
        for i in range(count):
            key, _, length, _, offset = struct.unpack_from('<HHIII', raw, 20 + i * 16)
            key += keys
            field = raw[key:raw.index(b'\0', key)].decode()
            fields[field] = raw[values + offset:values + offset + length].rstrip(b'\0')
        if (fields.get('TITLE_ID') != b'FNAFSCN01' or
                fields.get('APP_VER') != APP_VERSION.encode() or
                fields.get('TITLE') != APP_TITLE.encode()):
            raise ValueError('Wrong title, Title ID or APP_VER')

        return {
            'file': path.name,
            'bytes': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'app_version': APP_VERSION,
            'release_version': VERSION,
            'archive_crc': 'passed',
            'media_policy': 'Playable game data is prepared separately.',
            'entries': sorted(names),
        }


def package(launcher_vpk=None):
    if launcher_vpk is None:
        prepare()
    out = ROOT / 'out'
    out.mkdir(exist_ok=True)
    target = out / NAME
    pending = target.with_suffix('.vpk.pending')
    launcher = ZipFile(launcher_vpk) if launcher_vpk else nullcontext()
    with launcher as art, ZipFile(pending, 'w', ZIP_DEFLATED, compresslevel=9) as archive:
        archive.write(ROOT / 'build/scene/eboot.bin', 'eboot.bin')
        archive.write(ROOT / 'build/scene/param.sfo', 'sce_sys/param.sfo')
        if art is not None:
            for name in sorted(LAUNCHER_FILES):
                archive.writestr(name, art.read(name))
        else:
            for name, filename in LAUNCHER_ART.items():
                archive.write(ROOT / 'assets' / filename, name)
            archive.writestr(LIVEAREA + 'template.xml', DEFAULT_TEMPLATE)
        for notice in sorted((ROOT / 'third_party/licenses').glob('*.txt')):
            archive.write(notice, 'licenses/' + notice.name)
    verify(pending)
    pending.replace(target)
    report = verify(target)
    (out / 'public-build-report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    (out / (NAME + '.sha256')).write_text(f"{report['sha256']}  {NAME}\n", encoding='ascii')
    print(json.dumps(report, indent=2))
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launcher-vpk', type=Path, help='Read launcher art from an existing VPK.')
    package(parser.parse_args().launcher_vpk)
