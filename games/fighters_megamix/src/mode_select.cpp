// SPDX-License-Identifier: GPL-3.0-only
// Extend the game's original MODE SELECT, not the frontend's pause menu.
// The seven original choices keep their indexes; index 7 is the inserted Gallery.
#include "gallery.h"
#include "game.h"
#include "lift.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {
constexpr uint32_t kCursor=0x06084058,kMode=0x060e105c,kTransition=0x060944b8;
int seen=0,shown=-1;bool pending=false,loaded=false;
std::array<satrecomp::HudSprite,2> artwork;
bool LoadArtwork(){
    if(loaded)return true;
    std::ifstream file(std::filesystem::path(SDL_GetBasePath())/"gallery/gallery-entry.fmui",std::ios::binary);
    char magic[8];uint32_t width=0,height=0;
    file.read(magic,8);file.read(reinterpret_cast<char*>(&width),4);file.read(reinterpret_cast<char*>(&height),4);
    if(!file||memcmp(magic,"FMGMENU1",8)||!width||width>4096||!height||height>1024)return false;
    for(auto &sprite:artwork){
        sprite.x=84.f/352;sprite.y=328.f/448;sprite.width=(width/8.f)/704;sprite.height=48.f/448;
        sprite.aspect=4.f/3;
        sprite.columns=width;sprite.lines=height;sprite.pixels.resize(size_t(width)*height);
        file.read(reinterpret_cast<char*>(sprite.pixels.data()),sprite.pixels.size()*4);
        if(!file)return false;
    }
    loaded=true;return true;
}
bool MenuInput(satrecomp::Guest &g){
    if(g.Read32(kMode)!=20||g.PR()!=0x06037690||!LoadArtwork())return false;
    seen=3;
    if(g.Read8(kTransition))return false;
    uint8_t cursor=g.Read8(kCursor);
    // Called by the original input routine just after its repeat-key processing,
    // before drawing the rows. Its caller's stack still holds player 2's press.
    uint16_t pressed=uint16_t(g.R(14))|g.Read16(g.R(15)+2);
    uint16_t direction=pressed|g.Read16(0x06094900)|g.Read16(0x06094902);
    if(getenv("FMM_MODE_DEBUG"))fprintf(stderr,"mode input cursor=%u pressed=%04x direction=%04x repeat=%04x/%04x\n",cursor,pressed,direction,g.Read16(0x06094900),g.Read16(0x06094902));
    bool handled=cursor==7;
    if(cursor==4&&(direction&0x2000)&&!(direction&0x1000)){cursor=7;handled=true;}
    else if(cursor==5&&(direction&0x1000)){cursor=7;handled=true;}
    else if(cursor==7){
        if(direction&0x1000)cursor=4;
        else if(direction&0x2000)cursor=5;
        else if(pressed&0x0100){
            // Original B/cancel behavior (back to the title) still belongs to the game.
            g.Write8(kCursor,4);return false;
        }
        else if(pressed&(0x0400|0x0200|0x0800)){
            if(!satrecomp::RunningAhead())pending=true;
        }
    }
    if(!handled)return false;
    const bool moved=cursor!=g.Read8(kCursor);
    g.Write8(kCursor,cursor);
    uint32_t saved[15];for(unsigned i=0;i<15;i++)saved[i]=g.R(i);
    if(moved)g.Call(0x06014d30); // the original menu's navigation sound
    for(unsigned i=0;i<15;i++)g.R(i)=saved[i];
    // Consume only the input handled for the extra row. The original drawing and
    // caller then continue normally; index 7 leaves its seven rows unselected.
    g.R(14)=0;g.Write16(g.R(15)+2,0);
    g.Write16(0x06094900,0);g.Write16(0x06094902,0);
    return false;
}
}
namespace satrecomp {
void RegisterModeSelectGallery(){
    static const Lift hook={"MODE SELECT Gallery",0x06037a54,32,0x7E3947252B71F465ull,MenuInput};
    RegisterLifts(&hook,1);
}
void ModeSelectGalleryAfterFrame(){
    bool active=seen>0&&Ram32(kMode)==20&&Ram(kTransition)&&!*Ram(kTransition);
    if(seen)seen--;
    int wanted=active&&LoadArtwork()?(Ram(kCursor)&&*Ram(kCursor)==7?1:0):-1;
    if(wanted!=shown){
        shown=wanted;
        if(shown>=0)SetHudSprites({artwork[shown]});else SetHudSprites({});
    }
    if(pending&&!RunningAhead()){
        pending=false;
        if(active){
            fprintf(stderr,"MODE SELECT: opening Gallery in game\n");
            OpenGalleryCatalog();
        }
    }
}
}
