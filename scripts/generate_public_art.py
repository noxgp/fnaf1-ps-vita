"""Original text-only launcher artwork; no game images are read."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
FONT=ROOT/'third_party/fonts/Cousine-Regular.ttf'

def make(size,icon=False):
    w,h=size;im=Image.new('RGB',size,'#10151c');d=ImageDraw.Draw(im)
    d.rectangle((3,3,w-4,h-4),outline='#d84d4d',width=max(2,w//160))
    if icon:
        for text,y,sz,color in [('NOX',18,31,'#d84d4d'),('FNAF 1',62,21,'white'),('VITA',93,18,'#c3ccd9')]:
            d.text((w/2,y),text,font=ImageFont.truetype(str(FONT),sz),anchor='mt',fill=color)
    else:
        for text,y,sz,color in [('FNAF 1',h*.18,max(18,w//10),'white'),('PS VITA / NOX',h*.49,max(12,w//25),'#d84d4d'),('UNOFFICIAL NATIVE ADAPTATION',h*.70,max(9,w//40),'#c3ccd9')]:
            d.text((w/2,y),text,font=ImageFont.truetype(str(FONT),sz),anchor='mt',fill=color)
    return im

def prepare():
    out=ROOT/'assets';out.mkdir(exist_ok=True)
    for name,size,icon in [('icon0.png',(128,128),True),('background.png',(840,500),False),('startup.png',(280,158),False)]:
        make(size,icon).quantize(colors=128).save(out/name)
    (out/'launcher.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="840" height="500" viewBox="0 0 840 500"><rect width="840" height="500" fill="#10151c"/><rect x="3" y="3" width="834" height="494" fill="none" stroke="#d84d4d" stroke-width="5"/><g text-anchor="middle" font-family="Cousine,monospace"><text x="420" y="160" font-size="84" fill="white">FNAF 1</text><text x="420" y="280" font-size="33" fill="#d84d4d">PS VITA / NOX</text><text x="420" y="380" font-size="21" fill="#c3ccd9">UNOFFICIAL NATIVE ADAPTATION</text></g></svg>\n',encoding='utf-8')

if __name__=='__main__':prepare()
