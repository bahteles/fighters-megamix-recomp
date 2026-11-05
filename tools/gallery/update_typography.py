#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Update typography in a player's local Gallery theme; no artwork is bundled."""
import argparse
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont, ImageFilter

def update(theme, serif_path, sans_path):
    data=theme.read_bytes()
    if data[:8]!=b'FMMGUI02':
        raise ValueError('Unsupported Gallery theme')
    count,=struct.unpack_from('<I',data,8);at=12;assets={}
    for _ in range(count):
        length,width,height=struct.unpack_from('<III',data,at);at+=12
        name=data[at:at+length].decode('ascii');at+=length
        size=width*height*4
        assets[name]=(width,height,data[at:at+size]);at+=size
    if at!=len(data):raise ValueError('Invalid Gallery theme length')
    serif=ImageFont.truetype(str(serif_path),192)
    sans=ImageFont.truetype(str(sans_path),144)
    def text(style,label,font,gold=False):
        box=font.getbbox(label);pad=8
        mask=Image.new('L',(box[2]-box[0]+2*pad,box[3]-box[1]+2*pad))
        ImageDraw.Draw(mask).text((pad-box[0],pad-box[1]),label,font=font,fill=255)
        image=Image.new('RGBA',mask.size)
        shadow=mask.filter(ImageFilter.GaussianBlur(2))
        image.paste((0,0,30,230),(2,3),shadow)
        color=Image.new('RGBA',mask.size,(242,242,250,255))
        if gold:
            draw=ImageDraw.Draw(color)
            for y in range(mask.height):
                t=max(0,min(1,(y-pad)/max(1,mask.height-2*pad)))
                top=(255,244,164);bottom=(218,164,27)
                draw.line((0,y,mask.width,y),fill=tuple(round(a+(b-a)*t) for a,b in zip(top,bottom))+(255,))
        image.paste(color,(0,0),mask)
        assets[style+':'+label]=(image.width,image.height,image.tobytes())
    names=[name[6:] for name in assets if name.startswith('serif:')]
    excluded={'Previous','Next','Auto Rotate','Wireframe','Full','Broken','Mode Select','HD','SD','On','Off','Not Applicable','Original'}
    for name in names:
        if name not in excluded and not name.startswith('Character:') and ' / ' not in name:
            text('serif','Character: '+name,serif)
    for label in ['Character','Auto-Rotate','Wireframe','Armor','Color']:text('gold',label,serif,True)
    labels=['Previous','Next','|','Auto-Rotate','Wireframe','On','Off','Full','Broken','Not applicable','Original','Mode Select','Esc: Back','C: Select character','A: Change armor','V / B: Next / Previous']
    labels+=['%02d / 32'%i for i in range(1,33)]
    labels += ['%d / %d'%(i,n) for n in range(1,9) for i in range(1,n+1)]
    labels+=['Default','A','B','C','D','E','F','G','H','Tab: Next   Space: Auto-rotate   C: Characters']
    for label in labels:
        text('ui',label,sans)
        text('choice',label,sans,True)
    if len(assets)>512:raise ValueError('Too many theme entries')
    result=bytearray(b'FMMGUI02'+struct.pack('<I',len(assets)))
    for name,(width,height,pixels) in assets.items():
        key=name.encode('ascii');result+=struct.pack('<III',len(key),width,height)+key+pixels
    theme.write_bytes(result)
    print('Updated Gallery typography:',len(assets),'entries')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('theme',type=Path)
    parser.add_argument('--serif',required=True,type=Path)
    parser.add_argument('--sans',required=True,type=Path)
    args=parser.parse_args()
    update(args.theme,args.serif,args.sans)
