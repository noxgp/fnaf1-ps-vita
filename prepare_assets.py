"""Prepare local game data from an owned APK. Never downloads game content."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import shutil
import struct
import sys
import zipfile

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'scripts'))
VERSION='1.0.6'
SUPPORTED_CCN_SHA256='70a1ec40284d4f5232d0ecedcbd94a68c51cd937983ad17d2cbf329aad35b6d4'
UI_VERSION=b'FNAF1-PUBLIC-UI-1\n'

def digest(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def embedded_files(data):
    from extract_apk import chunks
    raw=(data/'original/application.ccn').read_bytes()
    payload=next(payload for kind,_,payload in chunks(raw,16) if kind==0x2238)
    count=struct.unpack_from('<I',payload)[0];pos=4;out=data/'embedded';out.mkdir(exist_ok=True)
    if count!=55:raise ValueError('Unsupported subtitle bank')
    seen=set()
    for _ in range(count):
        n=struct.unpack_from('<H',payload,pos)[0];pos+=2
        full=payload[pos:pos+n*2].decode('utf-16le');pos+=n*2
        size=struct.unpack_from('<I',payload,pos)[0];pos+=4
        name=PureWindowsPath(full).name
        if not name.startswith('subtitles_') or not name.endswith('.txt') or name in seen or size>len(payload)-pos:
            raise ValueError('Invalid embedded subtitle entry')
        seen.add(name);(out/name).write_bytes(payload[pos:pos+size]);pos+=size
    if pos!=len(payload):raise ValueError('Unconsumed embedded bytes')

def verify_output(folder):
    manifest=json.loads((folder/'prepared-manifest.json').read_text(encoding='utf-8'))
    for item in manifest['files']:
        relative=Path(item['path'])
        if relative.is_absolute() or '..' in relative.parts:raise ValueError('Unsafe manifest path')
        p=folder/relative
        if not p.is_file() or p.stat().st_size!=item['bytes'] or digest(p)!=item['sha256']:
            raise ValueError('Missing or changed prepared file: '+item['path'])
    if (folder/'public-ui.version').read_bytes()!=UI_VERSION:raise ValueError('Wrong public UI revision')
    print(f"Verified {len(manifest['files'])} local files.",flush=True)
    return manifest

def resolve_fonts(font_dir=None):
    from PIL import ImageFont

    bundled = ROOT / "third_party/fonts/Cousine-Regular.ttf"
    folder = Path(font_dir).expanduser().resolve() if font_dir is not None else None
    if os.name == "nt":
        folder = folder or Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts"
        menu = folder / "consola.ttf"
        captions = {"default": folder / "tahoma.ttf", "ja-JP": folder / "YuGothM.ttc",
                    "zh-CHS": folder / "simsun.ttc", "ko-KR": folder / "malgun.ttf"}
        instructions = "Install the Windows language fonts or point --font-dir to them."
    else:
        folder = folder or ROOT / "fonts"
        menu = bundled
        captions = {"default": bundled, "ja-JP": folder / "NotoSansCJKjp-Regular.otf",
                    "zh-CHS": folder / "NotoSansCJKsc-Regular.otf",
                    "ko-KR": folder / "NotoSansCJKkr-Regular.otf"}
        instructions = "Download the Noto Sans CJK fonts listed in the macOS and Linux installation guide."

    for font in dict.fromkeys([menu, *captions.values(), bundled]):
        if not font.is_file():
            raise ValueError(f"Required font missing: {font}. {instructions}")
        try:
            ImageFont.truetype(str(font), 24)
        except OSError as exc:
            raise ValueError(f"Could not open font: {font}. {instructions}") from exc
    return menu, captions, bundled


def prepare(apk,output,work,ffmpeg=None,font_dir=None,ui_only=False):
    apk=Path(apk).resolve();output=Path(output).resolve();work=Path(work).resolve()
    if not apk.is_file():raise ValueError('APK file not found')
    if output.exists() and any(output.iterdir()):raise ValueError('Output must be empty; use a new folder. Do not overwrite saves.')
    if output==work or work in output.parents or output in work.parents:raise ValueError('Output and work folders must be separate')
    with zipfile.ZipFile(apk) as z:
        info=z.getinfo('res/raw/application.ccn')
        if info.file_size>128*1024*1024:raise ValueError('Application data too large')
        ccn_hash=hashlib.sha256(z.read(info)).hexdigest()
    if ccn_hash!=SUPPORTED_CCN_SHA256:
        raise ValueError('Unsupported game-data revision. Expected Android 2.0.7 CCN SHA-256 '+SUPPORTED_CCN_SHA256+'; got '+ccn_hash+'. Other APK revisions need a separate compatibility update.')
    menu_font, caption_fonts, native_font = resolve_fonts(font_dir)
    ffmpeg=str(Path(ffmpeg).resolve()) if ffmpeg else shutil.which('ffmpeg')
    if not ffmpeg or not Path(ffmpeg).is_file():raise ValueError('FFmpeg not found; add it to PATH or use --ffmpeg PATH')
    sentinel=work/'asset-work.json'
    if work.exists() and any(work.iterdir()):
        if not sentinel.is_file() or json.loads(sentinel.read_text()).get('ccn_sha256')!=ccn_hash:
            raise ValueError('Work folder is not a matching asset workspace; choose a new empty folder')
    work.mkdir(parents=True,exist_ok=True);(work/'build').mkdir(exist_ok=True)
    sentinel.write_text(json.dumps(dict(ccn_sha256=ccn_hash,tool_version=VERSION)),encoding='utf-8')
    os.environ['FNAF_FFMPEG']=ffmpeg
    os.environ['FNAF_NATIVE_FONT']=str(native_font)
    import extract_apk,decode_scenes,audit_events,compile_scenes
    import prepare_game,prepare_android_ui,prepare_presentation,prepare_sprite_atlas
    data=work/'data'
    print('1/6 Checking and extracting local APK data...',flush=True)
    if not (data/'report.json').is_file():extract_apk.extract(apk,data,previews=False)
    embedded_files(data);decode_scenes.decode(data);audit_events.audit(data)
    prepare_game.ROOT=work;prepare_android_ui.ROOT=work;prepare_presentation.ROOT=work
    prepare_presentation.OUT=work/'build/android-ui';prepare_presentation.OUT.mkdir(exist_ok=True)
    prepare_sprite_atlas.ROOT=work
    print('2/6 Preparing sprite and audio files...',flush=True)
    prepare_game.compile_rules();prepare_game.sprites();prepare_game.audio()
    print('3/6 Preparing UI and all subtitle languages...',flush=True)
    compile_scenes.compile_data(data,work/'build/generated_scene_data.h',[0,3,4,5])
    prepare_android_ui.prepare(menu_font)
    prepare_presentation.font_atlas()
    prepare_presentation.subtitle_pack(caption_fonts)
    prepare_presentation.scare_budget();prepare_presentation.cache_layout()
    ui=work/'build/android-ui';cues=json.loads((ui/'subtitles.json').read_text(encoding='utf-8'))
    cues.sort(key=lambda c:c['handle']);assert len(cues)==2180
    index=bytearray(struct.pack('<4sII',b'FCIX',1,len(cues)))
    for c in cues:index.extend(struct.pack('<IIHH',c['offset'],c['compressed_size'],c['width'],c['height']))
    (ui/'subtitles.idx').write_bytes(index)
    print('4/6 Copying prepared data to a new output folder...',flush=True)
    output.mkdir(parents=True,exist_ok=True)
    if not ui_only:
        shutil.copytree(data/'native/sprites',output/'sprites',dirs_exist_ok=True)
        shutil.copytree(data/'native/audio',output/'audio',dirs_exist_ok=True)
    (output/'ui').mkdir(exist_ok=True)
    manifest=json.loads((ui/'manifest.json').read_text(encoding='utf-8'));atlas=json.loads((ui/'sprite-atlas.json').read_text(encoding='utf-8'))
    for t in manifest['textures']+atlas['pages']:
        name=f"{t['handle']:04d}.rgba";shutil.copy2(ui/name,output/'ui'/name)
    for name in ('subtitles.pack','subtitles.idx','manifest.json','sprite-atlas.json'):
        shutil.copy2(ui/name,output/'ui'/name)
    (output/'data.version').write_bytes(b'FNAF1-SCENE-DATA-1\n')
    (output/'game.version').write_bytes(b'FNAF1-GAME-DATA-2\n')
    (output/'public-ui.version').write_bytes(UI_VERSION)
    files=[dict(path=p.relative_to(output).as_posix(),bytes=p.stat().st_size,sha256=digest(p)) for p in sorted(output.rglob('*')) if p.is_file()]
    report=dict(tool_version=VERSION,ccn_sha256=ccn_hash,ui_only=ui_only,private_user_data=True,files=files)
    (output/'prepared-manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('5/6 Validating copied files...',flush=True);verify_output(output)
    print('6/6 Exporting local-only build headers...',flush=True)
    headers=ROOT/'build';headers.mkdir(exist_ok=True)
    for p in (work/'build').glob('generated*.h'):shutil.copy2(p,headers/p.name)
    for name in ('cache-layout.json','scare-budget.json'):shutil.copy2(work/'build'/name,headers/name)
    print('Ready: '+str(output),flush=True)
    print('Copy this fnaf1 folder to ux0:data/. Keep progress.sav/progress.bak. Do not upload these game files.',flush=True)
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--apk',type=Path);p.add_argument('--output',type=Path,default=ROOT/'prepared/fnaf1')
    p.add_argument('--work-dir',type=Path,default=ROOT/'build/asset-work')
    p.add_argument('--ffmpeg',type=Path)
    p.add_argument('--font-dir',type=Path,help='Directory with the fonts named in the installation guide')
    p.add_argument('--ui-only',action='store_true',help='Prepare only the UI update for an existing complete v2 data installation')
    p.add_argument('--verify',type=Path,help='Verify an already prepared fnaf1 folder; no APK required')
    a=p.parse_args()
    try:
        if a.verify:verify_output(a.verify)
        elif a.apk:prepare(a.apk,a.output,a.work_dir,a.ffmpeg,a.font_dir,a.ui_only)
        else:p.error('--apk or --verify is required')
    except (ValueError,OSError,KeyError,zipfile.BadZipFile) as e:
        print('Preparation failed: '+str(e),file=sys.stderr);return 1
    return 0

if __name__=='__main__':raise SystemExit(main())
