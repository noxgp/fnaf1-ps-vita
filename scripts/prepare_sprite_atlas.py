"""Pack unchanged small game sprites into shared GPU allocations inside VPK.

Each logical sprite retains its native pixels, hotspot and drawing geometry.
The two-pixel extruded gutter prevents adjacent sprites bleeding with filtering.
"""
import json
import struct
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
WIDTH,HEIGHT,PAD,FIRST_PAGE=1024,512,2,7800

def prepare():
    objects={o['handle']:o for o in json.loads((ROOT/'data/scenes/objects.json').read_text(encoding='utf-8'))}
    handles=set()
    for obj in (59,62,66,88,89,106,113,115,119,125,126,131,139,140,141,163,*range(74,88)):
        for a in objects[obj]['common']['animations']:
            for d in a['directions']:handles.update(d['images'])
    handles.update((52,81,82,84,85,86,87,123,128,146,204,212,213,214,456,455))
    handles.update((249,252,253,254,255,256,257,258,259,260))
    handles.update((589,588,595,596,597,598,599,600,601,602))
    tiles={}
    for h in handles:
        raw=(ROOT/'data/native/sprites'/f'{h:04d}.rgba').read_bytes()
        magic,w,ht,_=struct.unpack_from('<4sHHI',raw)
        assert magic==b'FSPI' and len(raw)==12+w*ht*4
        assert w+PAD*2<=WIDTH and ht+PAD*2<=HEIGHT
        tiles[h]=Image.frombytes('RGBA',(w,ht),raw[12:])
    pages=[];entries=[]
    for h in sorted(handles,key=lambda h:(-tiles[h].height,-tiles[h].width,h)):
        tile=tiles[h];w,ht=tile.size;pw,ph=w+PAD*2,ht+PAD*2;chosen=None
        for page in pages:
            for row in page['rows']:
                if ph<=row['height'] and row['used']+pw<=WIDTH:
                    chosen=page,row;break
            if chosen:break
            if page['bottom']+ph<=HEIGHT:
                row=dict(y=page['bottom'],height=ph,used=0)
                page['rows'].append(row);page['bottom']+=ph;chosen=page,row;break
        if not chosen:
            row=dict(y=0,height=ph,used=0)
            page=dict(handle=FIRST_PAGE+len(pages),rows=[row],bottom=ph,image=Image.new('RGBA',(WIDTH,HEIGHT)))
            pages.append(page);chosen=page,row
        page,row=chosen;x,y=row['used']+PAD,row['y']+PAD;row['used']+=pw
        padded=tile.resize((w+PAD*2,ht+PAD*2),Image.Resampling.NEAREST)
        padded.paste(tile,(PAD,PAD))
        # Copy actual edge texels into the gutter, preserving straight alpha.
        padded.paste(tile.crop((0,0,1,ht)).resize((PAD,ht)),(0,PAD))
        padded.paste(tile.crop((w-1,0,w,ht)).resize((PAD,ht)),(PAD+w,PAD))
        padded.paste(tile.crop((0,0,w,1)).resize((w,PAD)),(PAD,0))
        padded.paste(tile.crop((0,ht-1,w,ht)).resize((w,PAD)),(PAD,PAD+ht))
        for dx,dy,sx,sy in ((0,0,0,0),(PAD+w,0,w-1,0),(0,PAD+ht,0,ht-1),(PAD+w,PAD+ht,w-1,ht-1)):
            padded.paste(tile.getpixel((sx,sy)),(dx,dy,dx+PAD,dy+PAD))
        page['image'].paste(padded,(x-PAD,y-PAD))
        entries.append(dict(handle=h,page=page['handle'],x=x,y=y,width=w,height=ht))
    output=ROOT/'build/android-ui';output.mkdir(exist_ok=True)
    page_defs=[]
    for p in pages:
        page_defs.append(dict(handle=p['handle'],width=WIDTH,height=HEIGHT))
        (output/f"{p['handle']:04d}.rgba").write_bytes(struct.pack('<4sHHI',b'FSPI',WIDTH,HEIGHT,0)+p['image'].tobytes())
    for e in entries:
        page=pages[e['page']-FIRST_PAGE]['image']
        assert page.crop((e['x'],e['y'],e['x']+e['width'],e['y']+e['height'])).tobytes()==tiles[e['handle']].tobytes()
    entries.sort(key=lambda e:e['handle'])
    manifest=dict(pages=page_defs,sprites=entries,gutter=PAD,source_pixels_unchanged=True)
    (output/'sprite-atlas.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    lines=['/* Generated: unchanged sprite pixels in shared GPU pages. */',
        'static const SpriteAtlasItem sprite_atlas_items[]={'+','.join('{%d,%d,%d,%d,%d,%d}'%tuple(e[k] for k in ('handle','page','x','y','width','height')) for e in entries)+'};',
        'static const SpriteAtlasPage sprite_atlas_pages[]={'+','.join('{%d,%d,%d}'%tuple(p[k] for k in ('handle','width','height')) for p in page_defs)+'};']
    (ROOT/'build/generated_sprite_atlas.h').write_text('\n'.join(lines)+'\n',encoding='ascii')
    print(f'Packed {len(entries)} unchanged small sprites into {len(pages)} shared GPU pages ({len(pages)*WIDTH*HEIGHT*4:,} bytes)')
    return manifest

if __name__=='__main__':prepare()
