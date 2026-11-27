"""APK fonts and subtitle layout without modifying installed game files.

Font bank: Consolas 24 (native overlays), Tahoma 32 (phone/sign captions).
Rasterized glyphs only; no system font binaries are distributed.
"""
import json
import os
import re
import struct
import zlib
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from prepare_game import LANGS, sections
from prepare_sprite_atlas import prepare as prepare_sprite_atlas

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/android-ui'

def font_atlas():
    font=ImageFont.truetype(os.environ.get('FNAF_NATIVE_FONT','C:/Windows/Fonts/consola.ttf'),17)
    atlas=Image.new('L',(512,72));draw=ImageDraw.Draw(atlas)
    for i,ch in enumerate(''.join(chr(c) for c in range(32,127))+'©'):
        draw.text(((i%32)*16,(i//32)*24),ch,font=font,fill=255)
    raw=atlas.tobytes();runs=[];start=0
    while start<len(raw):
        end=start+1
        while end<len(raw) and raw[end]==raw[start] and end-start<65535:end+=1
        runs.append((end-start,raw[start]));start=end
    (ROOT/'build/generated_native_font.h').write_text(
        'enum {FONT_W=512,FONT_H=72,FONT_CELL_W=16,FONT_CELL_H=24,FONT_ADVANCE=10};\n'
        'static const struct {unsigned short count;unsigned char alpha;} font_runs[]={'+
        ','.join('{%d,%d}'%r for r in runs)+'};\n',encoding='ascii')
    atlas.save(OUT/'native-font.png')

def wrap(block,font):
    rows=[]
    for line in block.split('\n'):
        current=''
        for word in line.split(' '):
            next_line=(current+' '+word).strip()
            if current and font.getlength(next_line)>726:rows.append(current);current=word
            else:current=next_line
        # Character wrapping for languages with no word separators.
        while font.getlength(current)>726:
            end=1
            while end<len(current) and font.getlength(current[:end+1])<=726:end+=1
            rows.append(current[:end]);current=current[end:]
        rows.append(current)
    return '\n'.join(rows)

def subtitle_pack(font_paths=None):
    cues=[];handle=1024;packed=bytearray();shared={}
    for language,lang in enumerate(LANGS):
        if font_paths is None:
            names = {'ja-JP': 'YuGothM.ttc', 'zh-CHS': 'simsun.ttc', 'ko-KR': 'malgun.ttf'}
            font_path = Path(os.environ.get('FNAF_FONT_DIR', 'C:/Windows/Fonts')) / names.get(lang, 'tahoma.ttf')
        else:
            font_path = font_paths.get(lang, font_paths['default'])
        font = ImageFont.truetype(str(font_path), 24)
        for track,name in enumerate(['forced','voiceover1c','voiceover2a','voiceover3','voiceover4']):
            entries=[]
            content=sections((ROOT/'data/embedded'/f'subtitles_{lang}_{name}.txt').read_text(encoding='utf-8-sig'))
            for key,values in content.items():
                if track and 'sta' not in values:continue
                lines=[(int(k[3:]),v) for k,v in values.items() if re.fullmatch('dat[1-9][0-9]*',k)]
                if not any(v for _,v in lines):continue
                if track:blocks=['\n'.join(v for _,v in sorted(lines) if v)]
                else:blocks=['\n'.join([values.get(f'dat{i}',''),values.get(f'dat{i+1}','')]).strip() for i in range(1,max(i for i,_ in lines)+1,2)]
                for step,block in enumerate(blocks):
                    if not block:continue
                    block=wrap(block,font)
                    scratch=ImageDraw.Draw(Image.new('L',(1,1)))
                    box=scratch.multiline_textbbox((0,0),block,font=font,spacing=6,align='center')
                    w=int(min(750,max(1,box[2]-box[0]+24)));h=int(max(1,box[3]-box[1]+16))
                    mask=Image.new('L',(w,h));draw=ImageDraw.Draw(mask)
                    draw.multiline_text((w/2,8-box[1]),block,font=font,anchor='ma',spacing=6,align='center',fill=255)
                    compressed=zlib.compress(mask.tobytes(),9);identity=(w,h,compressed)
                    if identity not in shared:shared[identity]=len(packed);packed.extend(compressed)
                    stamp=int(values.get('sta','0'))
                    entries.append(dict(language=language,track=track,time=stamp,end=stamp+6000,handle=handle,width=w,height=h,key=key,step=step,enabled=not key.startswith('/'),offset=shared[identity],compressed_size=len(compressed)))
                    handle+=1
            if track:
                entries.sort(key=lambda c:c['time'])
                for a,b in zip(entries,entries[1:]):a['end']=b['time']
            cues.extend(entries)
    old=None
    old_path=ROOT/'data/native/subtitles/index.json'
    if old_path.exists():old=json.loads(old_path.read_text(encoding='utf-8'))
    assert old is None or [(c['handle'],c['key'],c['step'],c['language'],c['track']) for c in cues]==[(c['handle'],c['key'],c['step'],c['language'],c['track']) for c in old]
    (OUT/'subtitles.pack').write_bytes(packed)
    (OUT/'subtitles.json').write_text(json.dumps(cues,ensure_ascii=False),encoding='utf-8')
    # Validate every stored range and decompressed length before it enters a VPK.
    for c in cues:assert len(zlib.decompress(packed[c['offset']:c['offset']+c['compressed_size']]))==c['width']*c['height']
    first=next(c for c in cues if c['handle']==1024)
    fixture=packed[first['offset']:first['offset']+first['compressed_size']]
    mask=zlib.decompress(fixture);white=mask.index(255)
    (ROOT/'build/generated_caption_fixture.h').write_text(
        'enum {CAPTION_WIDTH=%d,CAPTION_HEIGHT=%d,CAPTION_WHITE=%d};\n'%(first['width'],first['height'],white)+
        'static const unsigned char caption_fixture[]={'+','.join(map(str,fixture))+'};\n',encoding='ascii')
    fields=('language','track','time','end','handle','width','height')
    lines=['typedef struct {int language,track,time,end,handle,width,height;} SubtitleCue;']
    lines.append('static const SubtitleCue subtitle_cues[]={'+','.join('{'+','.join(str(c[k]) for k in fields)+'}' for c in cues if c['track'])+'};')
    lines.append('typedef struct {int language,step,handle,width,height,enabled;const char *key;} ForcedCue;')
    lines.append('static const ForcedCue forced_cues[]={'+','.join('{'+','.join(str(int(c[k])) for k in ('language','step','handle','width','height','enabled'))+','+json.dumps(c['key'].lstrip('/'))+'}' for c in cues if not c['track'])+'};')
    (ROOT/'build/generated_game_text.h').write_text('\n'.join(lines),encoding='ascii')
    lines=['typedef struct {unsigned offset,size;unsigned short width,height;} PackedSubtitle;',
           'static const PackedSubtitle packed_subtitles[]={'+','.join('{%d,%d,%d,%d}'%tuple(c[k] for k in ('offset','compressed_size','width','height')) for c in sorted(cues,key=lambda c:c['handle']))+'};']
    (ROOT/'build/generated_subtitle_pack.h').write_text('\n'.join(lines),encoding='ascii')
    print(f'Subtitles prepared; {len(cues)} caption masks in {len(packed):,} bytes, existing data unchanged')

def scare_budget():
    report=json.loads((ROOT/'data/report.json').read_text(encoding='utf-8'))
    images={i['handle']:i for i in report['images']};budgets=[]
    for obj in json.loads((ROOT/'data/scenes/objects.json').read_text(encoding='utf-8')):
        if obj['type']==45:
            raw=bytes.fromhex(obj['common']['extension_hex']);count=struct.unpack_from('<H',raw,44)[0]
            frames=struct.unpack_from(f'<{count}H',raw,46);speed=struct.unpack_from('<I',raw,36)[0]
        elif obj['handle']==164:
            a=obj['common']['animations'][0]['directions'][0]
            frames=a['images'];count=len(frames);speed=a['minimum_speed']
            assert speed==a['maximum_speed']==60 and a['repeat']==1 and count==21
            assert all(len(images[h]['hotspot'])==2 for h in frames)
            (ROOT/'build/generated_power_out.h').write_text(
                '/* Scene 08-freddy, Active 164: separate power-out attack. */\n'+
                'static const int power_out_frames[]={'+','.join(map(str,frames))+'};\n'+
                f'enum {{POWER_OUT_COUNT={count},POWER_OUT_SPEED={speed}}};\n',encoding='ascii')
        else:continue
        before=after=0
        for handle in set(frames):
            m=images[handle];w=(m['width']*3+2)//4;h=(m['height']*3+2)//4
            before+=((((w+7)&~7)*4*h)+0x3ffff)&~0x3ffff
            w=(w*2+1)//3;h=(h*2+1)//3
            after+=((((w+7)&~7)*4*h)+0x3ffff)&~0x3ffff
        # Leave space for the office, fallback rendering and other demanded UI.
        assert after<48*1024*1024
        budgets.append(dict(object=obj['handle'],frames=count,previous_gpu_bytes=before,resident_gpu_bytes=after,speed_percent=speed))
    assert len(budgets)==5
    (ROOT/'build/scare-budget.json').write_text(json.dumps(budgets,indent=2),encoding='utf-8')

def cache_layout():
    objects={o['handle']:o for o in json.loads((ROOT/'data/scenes/objects.json').read_text(encoding='utf-8'))}
    critical={591}
    for obj in (60,61):
        for a in objects[obj]['common']['animations']:
            if a['animation'] in (12,14):critical.update(a['directions'][0]['images'])
    # Keep monitor frames, camera indicators, map labels and office light overlays ready.
    # A press or an animatronic moving should never have to load an overlay.
    for obj in (50,57,69,73,*range(74,88),134):
        for a in objects[obj]['common']['animations']:
            for direction in a['directions']:critical.update(direction['images'])
    atlas=prepare_sprite_atlas()
    critical.update(s['handle'] for s in atlas['sprites'])
    critical.update((395,396))
    noise=objects[9]['common']['animations'][0]['directions'][0]['images']
    assert len(noise)==8
    (ROOT/'build/generated_camera_noise.h').write_text('/* Original static frames, stored as GPU luminance. */\nstatic const int camera_noise_images[]={'+','.join(map(str,noise))+'};\n',encoding='ascii')
    up=objects[69]['common']['animations'][0]['directions'][0]
    down=objects[73]['common']['animations'][0]['directions'][0]
    assert up['images']==list(reversed(down['images'])) and len(up['images'])==11
    assert up['minimum_speed']==up['maximum_speed']==down['minimum_speed']==down['maximum_speed']==50
    images={i['handle']:i for i in json.loads((ROOT/'data/report.json').read_text(encoding='utf-8'))['images']}
    atlas_items={s['handle']:s['page'] for s in atlas['sprites']}
    atlas_pages={p['handle']:p for p in atlas['pages']}
    physical={atlas_items.get(h,h) for h in critical};budget=0
    for handle in physical:
        if handle in atlas_pages:w,h=atlas_pages[handle]['width'],atlas_pages[handle]['height']
        else:m=images[handle];w=(m['width']*3+2)//4;h=(m['height']*3+2)//4
        budget+=((((w+7)&~7)*4*h)+0x3ffff)&~0x3ffff
    assert budget<=45*1024*1024
    groups=json.loads((ROOT/'data/events/05.json').read_text(encoding='utf-8'))['groups'];plush={}
    for g in groups:
        for a in g['actions']:
            if (a['object_type'],a['opcode'])!=(-5,0):continue
            for p in a['parameters']:
                if p['code']!=9:continue
                raw=bytes.fromhex(p['payload_hex']);obj=struct.unpack_from('<H',raw,24)[0]
                if obj in (139,140,141,163):
                    assert struct.unpack_from('<H',raw)[0]==65535
                    plush[obj]=struct.unpack_from('<hh',raw,4)
    assert len(plush)==4
    lines=['/* Original creation anchors: room groups 799-802. */',
           'typedef struct {int object,x,y;} PlushPlacement;',
           'static const PlushPlacement plush_places[]={'+','.join('{%d,%d,%d}'%(k,*plush[k]) for k in (139,140,141,163))+'};',
           'static const int control_textures[]={'+','.join(map(str,sorted(critical)))+'};',
           f'enum {{CONTROL_TEXTURE_COUNT={len(critical)}}};']
    (ROOT/'build/generated_controls.h').write_text('\n'.join(lines),encoding='ascii')
    audio=[]
    for p in sorted((ROOT/'data/native/audio').glob('s*.*')):
        if p.suffix not in ('.wav','.ogg') or not p.stem[1:].isdigit():continue
        audio.append(dict(id=int(p.stem[1:]),vorbis=int(p.suffix=='.ogg'),bytes=p.stat().st_size))
    assert len(audio)==53 and len({a['id'] for a in audio})==53
    assert sum(a['bytes'] for a in audio)<=48*1024*1024
    (ROOT/'build/generated_audio_bank.h').write_text('static const AudioAsset audio_assets[]={'+','.join('{%d,%d,%d}'%(a['id'],a['vorbis'],a['bytes']) for a in audio)+'};\n',encoding='ascii')
    (ROOT/'build/cache-layout.json').write_text(json.dumps(dict(control_handles=sorted(critical),control_physical_handles=sorted(physical),control_gpu_bytes=budget,noise_handles=noise,noise_gpu_bytes=8*512*1024,atlas_pages=len(atlas_pages),plush=plush,audio_assets=audio,audio_ram_bytes=sum(a['bytes'] for a in audio)),indent=2),encoding='utf-8')
    print(f'Persistent controls/map/monitor: {len(critical)} textures, {budget:,} GPU bytes; audio RAM bank: {sum(a["bytes"] for a in audio):,} bytes')

if __name__=='__main__':
    OUT.mkdir(parents=True,exist_ok=True)
    font_atlas();subtitle_pack();scare_budget();cache_layout()
