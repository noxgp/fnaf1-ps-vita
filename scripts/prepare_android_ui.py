"""Prepare original Android menu geometry and text for the Vita frontend.

Existing APK sprite handles are reused without changing external game files.
Only rasterized String objects and native audio labels go inside the small VPK.
The APK's font bank specifies Consolas 24/32; no font binaries are distributed.
"""
import io
import os
import json
from pathlib import Path
import struct
import zipfile
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[1]
SCENES={0:{0},3:{9,10,11,12,13,14,17,18,20,21,22,23,24,32,33,34,36,37,40,41,42,43},
        4:{44,45,46},14:{168,169,170,171,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,197,198,199,200,202},
        19:{202,211,212,213,215,216,217,218,219,220,221,222,223},
        20:{202,232,233,234,235,236,237,238,239,240,241,242,244,245,246,247}}

def wrapped(value,font,width):
    lines=[]
    for line in value.replace('\r','').split('\n'):
        words=line.split(' ');current=''
        for word in words:
            candidate=(current+' '+word).strip()
            if current and font.getlength(candidate)>width:lines.append(current);current=word
            else:current=candidate
        lines.append(current)
    return '\n'.join(lines)

def prepare():
    objects={o['handle']:o for o in json.loads((ROOT/'data/scenes/objects.json').read_text(encoding='utf-8'))}
    output=ROOT/'build/android-ui';output.mkdir(parents=True,exist_ok=True)
    entries=[];widgets=[];captions={};handle=7000
    def bake(obj,paragraph,text,width,height,font_size,flags):
        nonlocal handle
        w=max(1,int(width*.75+.5));h=max(1,int(height*.75+.5))
        font=ImageFont.truetype(str(Path(os.environ.get('FNAF_FONT_DIR','C:/Windows/Fonts'))/'consola.ttf'),int(font_size*.75+.5))
        value=wrapped(text,font,w)
        im=Image.new('RGBA',(w,h));draw=ImageDraw.Draw(im)
        box=draw.multiline_textbbox((0,0),value,font=font,spacing=2)
        tw,th=box[2]-box[0],box[3]-box[1];align=flags&3
        x=(w-tw)/2 if align==1 else w-tw if align==2 else 0
        y=(h-th)/2 if flags&8 else 0
        draw.multiline_text((x-box[0],y-box[1]),value,font=font,fill='white',spacing=2,align='center' if align==1 else 'right' if align==2 else 'left')
        path=output/f'{handle:04d}.rgba';path.write_bytes(struct.pack('<4sHHI',b'FSPI',w,h,0)+im.tobytes())
        entries.append(dict(object=obj,paragraph=paragraph,handle=handle,width=w,height=h,text=text))
        handle+=1
        return handle-1
    for scene,wanted in SCENES.items():
        data=json.loads((ROOT/'data/scenes'/f'{scene:02d}.json').read_text(encoding='utf-8'))
        for instance in data['instances']:
            obj=objects[instance['object_info']]
            if obj['handle'] not in wanted:continue
            widgets.append(dict(scene=scene,object=obj['handle'],x=instance['x'],y=instance['y']))
            text=obj.get('common',{}).get('text')
            if not text or any(e['object']==obj['handle'] for e in entries):continue
            for index,p in enumerate(text['paragraphs']):
                if obj['handle']==43:
                    for n in range(1,7):bake(43,n-1,f'Night {n}',text['width'],text['height'],24,p['flags'])
                else:bake(obj['handle'],index,'v 2.0.7' if obj['handle']==40 else p['value'],text['width'],text['height'],32 if p['font']==1 else 24,p['flags'])
    custom={
        'UI_AUDIO':('Audio',192,48,24,8),'UI_MUSIC':('Music',416,48,24,9),
        'UI_VOLUME':('Master Volume',416,48,24,9),'UI_DIAGNOSTICS':('Diagnostic HUD',416,48,24,9),
        'UI_4_3':('4:3',160,60,40,9),'UI_VITA':('Vita 0.07',192,32,16,8),
        'UI_LOCAL':('Vita extras',256,48,24,8),
        'UI_PORT_CREDIT':('Ported to PS Vita by nox',480,28,20,8),
    }
    for index,(name,(value,w,h,size,flags)) in enumerate(custom.items()):captions[name]=bake(9000+index,0,value,w,h,size,flags)
    for v in range(0,101,10):captions[f'UI_VOLUME_{v}']=bake(9100,v//10,str(v),192,60,40,9)
    font_report=[]
    # Build 296 font bank: count followed by 108-byte records (LOGFONTW).
    report=json.loads((ROOT/'data/report.json').read_text(encoding='utf-8'))
    raw=(ROOT/'data/original/application.ccn').read_bytes();chunk=next(c for c in report['chunks'] if c['id']=='0x6667')
    bank=raw[chunk['offset']+8:chunk['offset']+8+chunk['bytes']]
    count=struct.unpack_from('<I',bank)[0];assert len(bank)==4+count*108
    for i in range(count):
        pos=4+i*108;key=struct.unpack_from('<I',bank,pos)[0];height=struct.unpack_from('<i',bank,pos+16)[0];name=bank[pos+44:pos+108].decode('utf-16le').split('\0')[0]
        font_report.append(dict(handle=key,height=height,face=name))
    assert any(f==dict(handle=0,height=-24,face='Consolas') for f in font_report)
    lines=['/* Generated from original Android scene positions and text objects. */',
        'typedef struct {int scene,object,x,y;} AndroidWidget;',
        'static const AndroidWidget android_widgets[]={'+','.join('{%d,%d,%d,%d}'%tuple(w[k] for k in ('scene','object','x','y')) for w in widgets)+'};',
        'typedef struct {int object,paragraph,handle,width,height;} AndroidText;',
        'static const AndroidText android_texts[]={'+','.join('{%d,%d,%d,%d,%d}'%tuple(e[k] for k in ('object','paragraph','handle','width','height')) for e in entries)+'};',
        'enum {'+','.join(f'{k}={v}' for k,v in captions.items())+'};']
    (ROOT/'build/generated_android_ui.h').write_text('\n'.join(lines)+'\n',encoding='ascii')
    (output/'manifest.json').write_text(json.dumps(dict(fonts=font_report,widgets=widgets,textures=entries),indent=2,ensure_ascii=False),encoding='utf-8')
    print(f'Prepared {len(widgets)} original UI placements, {len(entries)} embedded text textures; external data unchanged.')

if __name__=='__main__':prepare()
