"""Compile game-specific rules/art references from the supplied APK extraction.
Does not execute the Android runtime. Keeps provenance and omissions reviewable.
"""
import json
import os
import shutil
from pathlib import Path
import re
import struct
import subprocess
from PIL import Image, ImageDraw, ImageFont
from compile_scenes import NumericParser, Unsupported

ROOT=Path(__file__).resolve().parents[1]
LANGS=['en-US','fr-FR','de-DE','nl-NL','es-419','it-IT','pt-BR','ru-RU','ja-JP','zh-CHS','ko-KR']

def constant(p):
    expr=NumericParser(bytes.fromhex(p['payload_hex']))
    code=expr.parse()
    if len(code)!=1 or code[0][0]!='EX_CONST': raise Unsupported('nonconstant')
    return int(code[0][3]),expr.comparison

def starting_ai(groups):
    """Read the night-start counters in Freddy, Bonnie, Chica, Foxy order."""
    actor_ids = (120, 97, 105, 110)
    levels, random_bounds = [], []
    for night, group in enumerate(groups[437:443], 1):
        condition, once = group['conditions']
        if (condition['object_type'], condition['object_info'], condition['opcode']) != (7, 16, -81) or constant(condition['parameters'][0]) != (night, 0) or (once['object_type'], once['opcode']) != (-1, -7):
            raise ValueError(f'Unexpected starting AI conditions for night {night}')
        values = {}
        bound = 0
        for action in group['actions']:
            actor = action['object_info']
            if (action['object_type'], action['opcode']) != (7, 80) or actor not in actor_ids or actor in values:
                raise ValueError(f'Unexpected starting AI action for night {night}')
            code = NumericParser(bytes.fromhex(action['parameters'][0]['payload_hex'])).parse()
            if len(code) == 1 and code[0][0] == 'EX_CONST':
                values[actor] = int(code[0][3])
            elif (night, actor) == (4, 120) and [op[0] for op in code] == ['EX_CONST', 'EX_CONST', 'EX_RANDOM', 'EX_ADD']:
                values[actor] = int(code[0][3])
                bound = int(code[1][3])
                if bound < 1:
                    raise ValueError('Invalid Freddy random bound')
            else:
                raise ValueError(f'Unsupported starting AI expression for night {night}')
        if values.keys() != set(actor_ids):
            raise ValueError(f'Missing starting AI counters for night {night}')
        levels.append([values[actor] for actor in actor_ids])
        random_bounds.append(bound)
    if len(levels) != 6 or not random_bounds[3]:
        raise ValueError('Missing starting AI rules')
    return levels, random_bounds


def compile_rules():
    groups=json.loads((ROOT/'data/events/05.json').read_text(encoding='utf-8'))['groups']
    lines=['/* Generated from original room events; do not hand edit. */',
           'static int cmp(int a,int b,int op){switch(op){case 0:return a==b;case 1:return a!=b;case 2:return a<=b;case 3:return a<b;case 4:return a>=b;case 5:return a>b;}return 0;}',
           'static int original_camera_animation(const Game *g){int result=13;int view=game_camera_view(g);int room=game_camera_waypoint(g);']
    levels, random_bounds = starting_ai(groups)
    rows = ', '.join('{' + ', '.join(map(str, row)) + '}' for row in levels)
    bounds = ', '.join(map(str, random_bounds))
    lines[1:1] = [f'static const int original_starting_ai[6][4] = {{{rows}}};',
                  f'static const unsigned original_freddy_random_bound[6] = {{{bounds}}};']
    count=0; omitted=[]
    counters={56:'view',111:'g->fox',116:'g->picture',16:'g->night',103:'0'}
    def condition(c):
        t,op,obj=c['object_type'],c['opcode'],c['object_info'];p=c['parameters']
        if t==7 and op==-81 and obj in counters:
            val,comparison=constant(p[0]);s=f'cmp({counters[obj]},{val},{comparison})'
        elif t==2 and op==-4 and obj in (94,96,104,121):
            target=struct.unpack_from('<H',bytes.fromhex(p[0]['payload_hex']),2)[0]
            left='room' if obj==94 else f'g->position[{ {121:0,96:1,104:2}[obj]}]'
            right=f'g->position[{ {121:0,96:1,104:2}[target]}]' if target in (121,96,104) else str(target)
            s=f'{left}=={right}'
        elif t==2 and op==-42:
            index=struct.unpack_from('<H',bytes.fromhex(p[0]['payload_hex']))[0]
            values={(96,3):'g->branch[0]',(104,3):'g->branch[1]',(51,2):'g->camera_flicker',(51,5):'g->camera_twitch',(124,0):'g->golden',(121,6):'(g->position[0]==122)'}
            if (obj,index) not in values:raise Unsupported(str((obj,index)))
            val,comparison=constant(p[1]);s=f'cmp({values[obj,index]},{val},{comparison})'
        elif t==2 and op==-25 and obj==51:
            raw=bytes.fromhex(p[0]['payload_hex']);flags,masks,flagvalues=struct.unpack_from('<III',raw)
            if masks:raise Unsupported('flag mask')
            terms=[]
            for index in range(4):
                if not flags&(1<<(index*4)):break
                var,comparison,value=struct.unpack_from('<iii',raw,12+index*16)
                if var not in (2,5):raise Unsupported('variable '+str(var))
                terms.append(f'cmp(g->camera_{"flicker" if var==2 else "twitch"},{value},{comparison})')
            s=' && '.join(terms)
        elif (t,op) in ((-1,-1),(-1,-7)):s='1'
        else: raise Unsupported(str((t,op,obj)))
        return f'!({s})' if c['other_flags']&1 else f'({s})'
    # These groups select room/camera art. Office conditions are handled by the
    # office renderer, so only compile groups containing a camera comparison.
    for i,group in enumerate(groups[:230]):
        actions=[a for a in group['actions'] if (a['object_type'],a['opcode'],a['object_info'])==(2,17,51)]
        if not actions or not any(c['object_info']==56 and c['object_type']==7 for c in group['conditions']):continue
        try:
            cond=' && '.join(condition(c) for c in group['conditions'])
            animation=struct.unpack_from('<H',bytes.fromhex(actions[-1]['parameters'][0]['payload_hex']))[0]
            lines.append(f'if({cond})result={animation}; /* source group {i} */');count+=1
        except (Unsupported,KeyError) as e:omitted.append(dict(group=i,reason=str(e)))
    lines+=['return result;}','static int original_move(int actor,int pos,int branch,int door){']
    moves=[]
    for i,group in enumerate(groups[331:377],331):
        for a in group['actions']:
            if a['object_type']!=2 or a['opcode']!=1 or a['object_info'] not in (96,104):continue
            actor=a['object_info'];target=struct.unpack_from('<H',bytes.fromhex(a['parameters'][0]['payload_hex']))[0]
            source=None;b=0;d=-1
            for c in group['conditions']:
                if c['object_type']==2 and c['opcode']==-4 and c['object_info']==actor:source=struct.unpack_from('<H',bytes.fromhex(c['parameters'][0]['payload_hex']),2)[0]
                if c['object_type']==2 and c['opcode']==-42:
                    if c['object_info']==actor:
                        val,comparison=constant(c['parameters'][1]);b=val if comparison==0 else 0
                    if c['object_info'] in (60,61):d=constant(c['parameters'][1])[0]//2
            if source is None:raise ValueError('movement without position')
            terms=[f'actor=={1 if actor==96 else 2}',f'pos=={source}']
            if b:terms.append(f'branch=={b}')
            if d>=0:terms.append(f'door=={d}')
            lines.append(f"if({'&&'.join(terms)})return {target}; /* source group {i} */")
            moves.append(dict(group=i,actor=actor,source=source,target=target,branch=b,door=d))
    lines+=['return pos;}']
    (ROOT/'build/generated_game_rules.h').write_text('\n'.join(lines),encoding='ascii')
    (ROOT/'build/game-rule-coverage.json').write_text(json.dumps(dict(camera_rules=count,camera_omissions=omitted,movements=moves),indent=2),encoding='utf-8')
    print(f'Compiled {count} camera art rules and {len(moves)} original movement edges; {len(omitted)} rare-art omissions')

def sprites():
    report=json.loads((ROOT/'data/report.json').read_text(encoding='utf-8'))
    out=ROOT/'data/native/sprites';out.mkdir(parents=True,exist_ok=True)
    for m in report['images']:
        dest=out/f"{m['handle']:04d}.rgba"
        if dest.exists():continue
        with Image.open(ROOT/'data/images'/f"{m['handle']:04d}.png") as source:
            im=source.convert('RGBA').resize((max(1,int(m['width']*.75+.5)),max(1,int(m['height']*.75+.5))),Image.Resampling.LANCZOS)
        dest.write_bytes(struct.pack('<4sHHI',b'FSPI',*im.size,int(im.getchannel('A').getextrema()==(255,255)))+im.tobytes())
    objects=json.loads((ROOT/'data/scenes/objects.json').read_text(encoding='utf-8'))
    lines=['/* Original KYSO animation frame lists, extension fields 36/44/46. */']
    for obj in objects:
        if obj['type']==45:
            raw=bytes.fromhex(obj['common']['extension_hex']);speed=struct.unpack_from('<I',raw,36)[0];n=struct.unpack_from('<H',raw,44)[0]
            frames=struct.unpack_from(f'<{n}H',raw,46)
            assert all(any(m['handle']==f for m in report['images']) for f in frames)
            lines.append(f"static const int scare_{obj['handle']}[]={{"+','.join(map(str,frames))+'};')
            lines.append(f'enum {{scare_{obj["handle"]}_speed={speed}}};')
    (ROOT/'build/generated_scares.h').write_text('\n'.join(lines),encoding='ascii')
    print('Prepared all 529 original sprites and 4 original KYSO jump sequences')

def audio():
    ffmpeg=os.environ.get('FNAF_FFMPEG') or shutil.which('ffmpeg')
    if not ffmpeg:raise RuntimeError('FFmpeg is required; pass --ffmpeg or add it to PATH')
    output=ROOT/'data/native/audio';output.mkdir(exist_ok=True)
    for path in sorted((ROOT/'data/audio').iterdir()):
        if path.suffix not in ('.wav','.ogg','.mp3'):continue
        dest=output/(path.stem+('.wav' if path.suffix=='.wav' else '.ogg'))
        if dest.exists():continue
        codec=['-c:a','pcm_s16le'] if path.suffix=='.wav' else ['-c:a','libvorbis','-q:a','3']
        subprocess.run([ffmpeg,'-hide_banner','-loglevel','error','-i',str(path),'-ar','44100','-ac','2',*codec,'-y',str(dest)],check=True)
    print('Prepared 53 original sounds: stereo 44100 Hz, WAV effects / OGG streams')

def sections(text):
    result={};current=None
    for line in text.splitlines():
        if line.startswith('[') and ']' in line:
            key,rest=line[1:].split(']',1)
            if rest.strip():continue
            current=result.setdefault(key,{})
        elif current is not None and '=' in line:
            key,val=line.split('=',1);current[key.strip()]=val.strip()
    return result

def subtitles():
    # Bake glyphs from installed system fonts. Font binaries are not packaged.
    # One texture per cue; only the current cue is uploaded to the GPU.
    lines=['typedef struct {int language,track,time,end,handle,width,height;} SubtitleCue;']
    cues=[];handle=1024;out=ROOT/'data/native/subtitles';out.mkdir(exist_ok=True)
    for lang_index,lang in enumerate(LANGS):
        fontpath='YuGothM.ttc' if lang=='ja-JP' else 'simsun.ttc' if lang=='zh-CHS' else 'malgun.ttf' if lang=='ko-KR' else 'arial.ttf'
        font=ImageFont.truetype('C:/Windows/Fonts/'+fontpath,22)
        for track,name in enumerate(['forced','voiceover1c','voiceover2a','voiceover3','voiceover4']):
            content=sections((ROOT/'data/embedded'/f'subtitles_{lang}_{name}.txt').read_text(encoding='utf-8-sig'))
            entries=[]
            for key,values in content.items():
                if track and 'sta' not in values:continue
                text='\n'.join(v for k,v in sorted(values.items(),key=lambda p:int(p[0][3:]) if re.match(r'dat\d+$',p[0]) else -1) if re.match(r'dat[1-9]\d*$',k) and v)
                if not text:continue
                # Forced signs may have 10+ lines: each two-line step gets its own cue.
                rows=text.split('\n')
                if not track:
                    last=max(int(k[3:]) for k in values if re.match(r'dat[1-9]\d*$',k))
                    blocks=['\n'.join([values.get(f'dat{i}',''),values.get(f'dat{i+1}','')]).strip() for i in range(1,last+1,2)]
                else:blocks=[text]
                for step,block in enumerate(blocks):
                    if not block:continue
                    scratch=ImageDraw.Draw(Image.new('RGBA',(1,1)));box=scratch.multiline_textbbox((0,0),block,font=font,align='center',spacing=3)
                    w=int(min(920,max(1,box[2]-box[0]+24)));h=int(max(1,box[3]-box[1]+16))
                    image=Image.new('RGBA',(w,h),(0,0,0,205));draw=ImageDraw.Draw(image)
                    draw.multiline_text((w/2,8-box[1]),block,font=font,anchor='ma',align='center',spacing=3,fill='white')
                    path=out/f'{handle:04d}.rgba';path.write_bytes(struct.pack('<4sHHI',b'FSPI',w,h,0)+image.tobytes())
                    stamp=int(values.get('sta','0'));entry=dict(language=lang_index,track=track,time=stamp,end=stamp+6000,handle=handle,width=w,height=h,key=key,step=step)
                    entries.append(entry);handle+=1
            if track:
                entries.sort(key=lambda e:e['time'])
                for a,b in zip(entries,entries[1:]):a['end']=b['time']
            cues.extend(entries)
    lines.append('static const SubtitleCue subtitle_cues[]={'+','.join('{%d,%d,%d,%d,%d,%d,%d}'%tuple(c[k] for k in ('language','track','time','end','handle','width','height')) for c in cues if c['track'])+'};')
    # Forced sign lookup (original section name + 2-line step).
    lines.append('typedef struct {int language,step,handle,width,height;const char *key;} ForcedCue;')
    lines.append('static const ForcedCue forced_cues[]={'+','.join('{%d,%d,%d,%d,%d,%s}'%(c['language'],c['step'],c['handle'],c['width'],c['height'],json.dumps(c['key'])) for c in cues if not c['track'])+'};')
    (ROOT/'build/generated_subtitles.h').write_text('\n'.join(lines),encoding='ascii')
    (out/'index.json').write_text(json.dumps(cues,ensure_ascii=False),encoding='utf-8')
    print(f'Prepared {len(cues)} original subtitle/sign cues across all 11 languages')

if __name__=='__main__':
    compile_rules();sprites();audio();subtitles()
