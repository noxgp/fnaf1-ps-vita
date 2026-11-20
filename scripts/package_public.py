"""Allowlisted public VPK: executable, original launcher and library notices."""
from pathlib import Path
import hashlib
import json
import struct
import zipfile
from generate_public_art import prepare
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.1.0-beta.1'
NAME=f'FNAF1-Vita-v{VERSION}.vpk'
ALLOWED_BASE={'eboot.bin','sce_sys/param.sfo','sce_sys/icon0.png',
    'sce_sys/livearea/contents/background.png','sce_sys/livearea/contents/startup.png','sce_sys/livearea/contents/template.xml'}

def verify(path):
    with zipfile.ZipFile(path) as z:
        if z.testzip():raise ValueError('Archive CRC failure')
        names=set(z.namelist())
        if not ALLOWED_BASE<=names or any(n not in ALLOWED_BASE and not(n.startswith('licenses/') and n.endswith('.txt') and '/' not in n[9:]) for n in names):
            raise ValueError('Unexpected public VPK member')
        if z.read('eboot.bin')[:4]!=b'SCE\0':raise ValueError('Not a Vita SELF')
        raw=z.read('sce_sys/param.sfo');_,_,keys,values,count=struct.unpack_from('<5I',raw);sfo={}
        for i in range(count):
            key,fmt,n,_,offset=struct.unpack_from('<HHIII',raw,20+i*16)
            name=raw[keys+key:raw.index(b'\0',keys+key)].decode();sfo[name]=raw[values+offset:values+offset+n].rstrip(b'\0')
        if sfo['TITLE_ID']!=b'FNAFSCN01' or sfo['APP_VER']!=b'00.17':raise ValueError('Wrong title/version')
        return dict(file=Path(path).name,bytes=Path(path).stat().st_size,sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
            app_version='00.17',release_version=VERSION,archive_crc='passed',media_policy='No APK, game images, audio, subtitles or external data files in this VPK',entries=sorted(names))

def package():
    prepare();out=ROOT/'out';out.mkdir(exist_ok=True);target=out/NAME;temp=target.with_suffix('.vpk.pending')
    with zipfile.ZipFile(temp,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        z.write(ROOT/'build/scene/eboot.bin','eboot.bin');z.write(ROOT/'build/scene/param.sfo','sce_sys/param.sfo')
        z.write(ROOT/'assets/icon0.png','sce_sys/icon0.png')
        for name in ('background.png','startup.png'):z.write(ROOT/'assets'/name,'sce_sys/livearea/contents/'+name)
        z.writestr('sce_sys/livearea/contents/template.xml','<?xml version="1.0" encoding="utf-8"?><livearea style="a1" format-ver="01.00" content-rev="1"><livearea-background><image>background.png</image></livearea-background><gate><startup-image>startup.png</startup-image></gate></livearea>')
        for p in sorted((ROOT/'third_party/licenses').glob('*.txt')):z.write(p,'licenses/'+p.name)
    verify(temp);temp.replace(target);report=verify(target)
    (out/'public-build-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    (out/(NAME+'.sha256')).write_text(report['sha256']+'  '+NAME+'\n',encoding='ascii')
    print(json.dumps(report,indent=2))
    return target

if __name__=='__main__':package()
