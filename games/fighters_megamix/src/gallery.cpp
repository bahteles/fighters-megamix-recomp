// SPDX-License-Identifier: GPL-3.0-only
// Experimental model gallery. Geometry is observed at Megamix's vertex transform;
// the original routine still runs unchanged. Nothing is written to guest memory.
#include "gallery.h"
#include "gallery_theme.h"
#include "game.h"
#include "lift.h"
#include "png_write.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {
using satrecomp::Guest;
struct Point { float x,y,z; };
struct Face { Point p[4]; unsigned texture; bool flipX,flipY; };
struct Texture { int w,h; std::vector<uint32_t> pixels; };
struct Model { std::vector<Face> faces; };
std::array<Model,2> models;
std::vector<Texture> textures;
std::map<uint64_t,unsigned> textureIds;
bool capturing=false, requested=false;
int captureFrames=0;
struct ColorVariant { std::filesystem::path path, broken; };
struct Entry { std::string name; std::vector<ColorVariant> colors; };
std::vector<Entry> catalog;
bool catalogMode=false;
bool LoadModel(const std::filesystem::path &path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)return false;
    auto word=[&](uint32_t &value){input.read(reinterpret_cast<char*>(&value),4);return bool(input);};
    uint32_t magic=0,nt=0;
    if(!word(magic)||magic!=0x31474d46||!word(nt)||!nt||nt>4096)return false;
    std::vector<Texture> loaded;size_t bytes=0;
    for(uint32_t i=0;i<nt;i++){
        uint32_t w=0,h=0;if(!word(w)||!word(h)||!w||!h||w>512||h>512)return false;
        bytes+=size_t(w)*h*4;if(bytes>32*1024*1024)return false;
        Texture texture{int(w),int(h),{}};texture.pixels.resize(size_t(w)*h);
        input.read(reinterpret_cast<char*>(texture.pixels.data()),texture.pixels.size()*4);
        if(!input)return false;loaded.push_back(std::move(texture));
    }
    uint32_t count=0;if(!word(count)||count<1||count>10000)return false;
    Model model;model.faces.reserve(count);
    for(uint32_t i=0;i<count;i++){
        uint8_t record[56];input.read(reinterpret_cast<char*>(record),56);if(!input)return false;
        Face face{};memcpy(face.p,record,48);memcpy(&face.texture,record+48,4);
        face.flipX=record[52]!=0;face.flipY=record[53]!=0;
        if(face.texture>=loaded.size())return false;
        for(auto &p:face.p)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||std::abs(p.x)>100000||std::abs(p.y)>100000||std::abs(p.z)>100000)return false;
        model.faces.push_back(face);
    }
    models[0]=std::move(model);models[1].faces.clear();textures=std::move(loaded);
    fprintf(stderr,"Gallery catalog: %s faces=%zu textures=%zu\n",path.filename().string().c_str(),models[0].faces.size(),textures.size());
    return true;
}
bool LoadCatalog(){
    catalog.clear();std::filesystem::path folder=std::filesystem::path(SDL_GetBasePath())/"gallery";
    if(const char *overridePath=getenv("FMM_GALLERY_CATALOG");overridePath&&*overridePath)folder=overridePath;
    std::ifstream input(folder/"catalog.txt");std::string line;
    while(std::getline(input,line)&&catalog.size()<64){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        size_t split=line.find('|');if(split==std::string::npos||split==0||split>48)continue;
        Entry entry;entry.name=line.substr(0,split);
        auto modelPath=[&](const std::string &name){
            std::filesystem::path file=name;
            return !file.has_parent_path()&&file.extension()==".fmg"&&std::filesystem::is_regular_file(folder/file)?folder/file:std::filesystem::path{};
        };
        // Pairs of full/broken files, one pair per original game color.
        std::vector<std::string> files;
        size_t at=split+1;
        while(files.size()<16){
            size_t end=line.find('|',at);files.push_back(line.substr(at,end==std::string::npos?end:end-at));
            if(end==std::string::npos)break;at=end+1;
        }
        for(size_t i=0;i<files.size();i+=2){
            auto full=modelPath(files[i]);
            if(!full.empty())entry.colors.push_back({full,i+1<files.size()?modelPath(files[i+1]):std::filesystem::path{}});
        }
        if(!entry.colors.empty())catalog.push_back(std::move(entry));
    }
    return !catalog.empty();
}
bool Valid(uint32_t p, uint32_t size) { return p>=0x06000000 && p<=0x06100000-size; }
float Fixed(uint32_t value) { return static_cast<int32_t>(value)/65536.f; }
uint32_t Color(uint16_t value) {
    auto expand=[](unsigned v){return (v<<3)|(v>>2);};
    return expand(value&31)|(expand((value>>5)&31)<<8)|(expand((value>>10)&31)<<16)|0xff000000;
}
bool Capture(Guest &g) {
    if (!capturing) return false;
    uint32_t data=g.R(4), matrix=g.R(6);
    if (!Valid(data,24)||!Valid(matrix,48))return false;
    uint32_t points=g.Read32(data),nv=g.Read32(data+4),polys=g.Read32(data+8),nf=g.Read32(data+12),attrs=g.Read32(data+16);
    // Fifteen live body-part PDATA records per fighter (24 bytes per record).
    int player=data>=0x0607a0d0&&data<0x0607a238 ? 0 : data>=0x0607a238&&data<0x0607a3a0 ? 1 : -1;
    if(player<0){
        if (getenv("FMM_GALLERY_DEBUG"))fprintf(stderr,"Gallery skipped data=%08x points=%08x count=%u/%u matrix=%08x\n",data,points,nv,nf,matrix);
        return false;
    }
    // Asset preparation only: observe the exact alternate PDATA records selected by
    // the game's DressFighter (0x0600BBD0). The running game's records stay untouched.
    if(const char *variant=getenv("FMM_GALLERY_ARMOR_CAPTURE");variant&&*variant){
        uint32_t character=g.Read8((player?0x0606731c:0x0606551c)+0x5f);
        if(character<32){
            uint32_t upper=g.Read32(0x0606c6ac+character*4),lower=g.Read32(0x0606c72c+character*4);
            uint32_t part=(data-(player?0x0607a238:0x0607a0d0))/24;
            uint32_t alternate=0,bank=player?0x3c0:0;
            bool broken=std::string(variant)=="broken";
            if(broken){
                if(part<9&&(upper&(1u<<part)))alternate=0x06098a3c+bank+part*24;
                if(part>=8&&(lower&(1u<<part)))alternate=0x06098afc+bank+(part-8)*24;
            }else if((part<9&&(upper&(1u<<part)))||(part>=8&&(lower&(1u<<part))))alternate=0x060988d4+bank+part*24;
            if(alternate){data=alternate;points=g.Read32(data);nv=g.Read32(data+4);polys=g.Read32(data+8);nf=g.Read32(data+12);attrs=g.Read32(data+16);}
        }
    }
    if(nv<3||nv>1024||!nf||nf>2048||!Valid(points,nv*12)||!Valid(polys,nf*20)||!Valid(attrs,nf*12))return false;
    float m[12];for(int i=0;i<12;i++)m[i]=Fixed(g.Read32(matrix+i*4));
    std::vector<Point> vertices(nv);
    for(unsigned i=0;i<nv;i++){
        float p[3];for(int j=0;j<3;j++)p[j]=Fixed(g.Read32(points+i*12+j*4));
        vertices[i]={m[0]*p[0]+m[1]*p[1]+m[2]*p[2]+m[3],m[4]*p[0]+m[5]*p[1]+m[6]*p[2]+m[7],m[8]*p[0]+m[9]*p[1]+m[10]*p[2]+m[11]};
    }
    for(unsigned i=0;i<nf;i++){
        uint32_t a=attrs+i*12;
        uint16_t mode=g.Read16(a),source=g.Read16(a+4),shape=g.Read16(a+6),flags=g.Read16(a+8),col=g.Read16(a+2);
        int w=(shape>>8)*8,h=shape&255;
        bool flat=(flags&7)==4;
        if(flat)w=h=1;
        if(!w||!h||w*h>65536)continue;
        Face face{};bool valid=true;
        for(int j=0;j<4;j++){unsigned v=g.Read16(polys+i*20+12+j*2)/16;if(v>=nv){valid=false;break;}face.p[j]=vertices[v];}
        if(!valid)continue;
        uint64_t key=uint64_t(mode)<<48|uint64_t(col)<<32|uint64_t(source)<<16|shape;
        auto found=textureIds.find(key);
        if(found==textureIds.end()){
            Texture tex{w,h,{}};tex.pixels.resize(w*h);
            int cm=(mode>>3)&7;
            for(int p=0;p<w*h;p++){
                unsigned address=source*8;uint16_t value=0;bool visible=true;
                if(flat)value=shape;
                else if(cm==5){value=g.Read16(0x25c00000+((address+2*p)&0x7fffe));visible=(value&0x8000)||(mode&64);}
                else if(cm<2){uint8_t byte=g.Read8(0x25c00000+((address+p/2)&0x7ffff));unsigned index=(byte>>(p%2?0:4))&15;value=cm==1?g.Read16(0x25c00000+(((col<<3)+index*2)&0x7fffe)):0;visible=index||(mode&64);}
                tex.pixels[p]=Color(value)&(visible?0xffffffff:0x00ffffff);
            }
            face.texture=static_cast<unsigned>(textures.size());textureIds[key]=face.texture;textures.push_back(std::move(tex));
        }else face.texture=found->second;
        face.flipX=(flags&0x10)!=0;face.flipY=(flags&0x20)!=0;
        models[player].faces.push_back(face);
    }
    return false;
}
void Show();
bool Request(){
    for(auto &model:models)model.faces.clear();textures.clear();textureIds.clear();
    capturing=requested=true;captureFrames=0;return true;
}
void Dump(){
    const char *path=getenv("FMM_GALLERY_DUMP");if(!path||!*path)return;
    FILE *f=fopen(path,"wb");if(!f)return;
    uint32_t magic=0x31474d46,nt=static_cast<uint32_t>(textures.size());
    fwrite(&magic,4,1,f);fwrite(&nt,4,1,f);
    for(auto &t:textures){fwrite(&t.w,4,1,f);fwrite(&t.h,4,1,f);fwrite(t.pixels.data(),4,t.pixels.size(),f);}
    for(auto &m:models){uint32_t n=static_cast<uint32_t>(m.faces.size());fwrite(&n,4,1,f);fwrite(m.faces.data(),sizeof(Face),n,f);}
    fclose(f);
}
void Show(){
    fprintf(stderr,"Gallery: P1=%zu P2=%zu textures=%zu\n",models[0].faces.size(),models[1].faces.size(),textures.size());
    if(const char *only=getenv("FMM_GALLERY_CAPTURE_ONLY");only&&*only)return;
    if(models[0].faces.empty()&&models[1].faces.empty()){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,"Model Gallery","The model catalog is unavailable. Keep the gallery folder beside the game executable.",nullptr);return;
    }
    bool ownVideo=(SDL_WasInit(SDL_INIT_VIDEO)&SDL_INIT_VIDEO)==0;
    if(ownVideo&&!SDL_InitSubSystem(SDL_INIT_VIDEO))return;
    const char *shot=getenv("FMM_GALLERY_SHOT");
    if(shot&&!*shot)shot=nullptr;
    const bool embedded=satrecomp::TheGame().modalWindow&&bool(satrecomp::TheGame().presentModal);
    SDL_Window *window=embedded?satrecomp::TheGame().modalWindow:SDL_CreateWindow("Fighters Megamix | Model Gallery",1280,720,SDL_WINDOW_RESIZABLE|(shot?SDL_WINDOW_HIDDEN:0));
    if(!window){fprintf(stderr,"Gallery window: %s\n",SDL_GetError());if(ownVideo)SDL_QuitSubSystem(SDL_INIT_VIDEO);return;}
    SDL_Surface *canvas=embedded?SDL_CreateSurface(1280,720,SDL_PIXELFORMAT_RGBA32):nullptr;
    if(!embedded)SDL_SetWindowMinimumSize(window,960,540);
    SDL_Renderer *renderer=embedded?(canvas?SDL_CreateSoftwareRenderer(canvas):nullptr):SDL_CreateRenderer(window,nullptr);
    if(!renderer){if(canvas)SDL_DestroySurface(canvas);if(!embedded)SDL_DestroyWindow(window);if(ownVideo)SDL_QuitSubSystem(SDL_INIT_VIDEO);return;}
    if(!embedded)SDL_SetRenderVSync(renderer,1);
    fprintf(stderr,"Gallery presentation: %s window=%u\n",embedded?"in game":"headless test",SDL_GetWindowID(window));
    GalleryTheme theme(renderer);
    if(!theme.Load()){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Gallery","Gallery artwork is unavailable. Keep the gallery folder beside the game.",window);
        theme.Clear();SDL_DestroyRenderer(renderer);if(canvas)SDL_DestroySurface(canvas);
        if(!embedded)SDL_DestroyWindow(window);if(ownVideo)SDL_QuitSubSystem(SDL_INIT_VIDEO);return;
    }
    std::vector<SDL_Texture*> gpuTextures;
    bool hd=satrecomp::FeatureOn("hd_texture_pack")&&bool(satrecomp::TheGame().modelHdTexture);
    auto upload=[&]{
    for(auto *texture:gpuTextures)if(texture)SDL_DestroyTexture(texture);gpuTextures.clear();
    unsigned replacements=0;
    for(auto &t:textures){
        std::vector<uint32_t> pixels;
        bool replaced=hd&&satrecomp::TheGame().modelHdTexture&&satrecomp::TheGame().modelHdTexture(t.w,t.h,t.pixels.data(),pixels);
        int width=t.w*(replaced?4:1),height=t.h*(replaced?4:1);replacements+=replaced;
        SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,width,height);
        if(texture){SDL_UpdateTexture(texture,nullptr,replaced?pixels.data():t.pixels.data(),width*4);SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(texture,replaced?SDL_SCALEMODE_LINEAR:SDL_SCALEMODE_NEAREST);}
        gpuTextures.push_back(texture);
    }
    fprintf(stderr,"Gallery textures: %s replacements=%u/%zu\n",hd?"HD":"SD",replacements,textures.size());
    };upload();
    int gamepadCount=0;SDL_JoystickID *ids=SDL_GetGamepads(&gamepadCount);std::vector<SDL_Gamepad*> pads;
    for(int i=0;i<gamepadCount;i++)if(auto *pad=SDL_OpenGamepad(ids[i]))pads.push_back(pad);
    SDL_free(ids);
    float yaw=catalogMode?1.57f:0,pitch=0,zoom=1,panX=0,panY=0;
    if(const char *angle=getenv("FMM_GALLERY_ANGLE"))sscanf(angle,"%f,%f",&yaw,&pitch);
    int selected=models[0].faces.empty()?1:0;
    if(!catalogMode)if(const char *player=getenv("FMM_GALLERY_PLAYER"))selected=atoi(player)==2?1:0;
    bool running=true,spin=false,wire=false,drag=false,pan=false,picker=false,armorFull=true;unsigned frame=0;
    int pickCursor=selected,colorIndex=0;
    auto reset=[&]{yaw=catalogMode?1.57f:0;pitch=panX=panY=0;zoom=1;};
    auto change=[&](int delta){
        int next=catalogMode?(selected+delta+int(catalog.size()))%int(catalog.size()):selected^1;
        if(catalogMode&&!LoadModel(catalog[next].colors[0].path)){
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Gallery","Could not load this model.",window);return;
        }
        selected=next;reset();if(catalogMode)upload();
        armorFull=true;colorIndex=0;
    };
    auto armor=[&]{
        if(!catalogMode||catalog[selected].colors[colorIndex].broken.empty())return;
        auto &entry=catalog[selected].colors[colorIndex];
        if(LoadModel(armorFull?entry.broken:entry.path)){armorFull=!armorFull;upload();}
    };
    auto color=[&](int delta){
        if(!catalogMode)return;
        auto &variants=catalog[selected].colors;
        int next=(colorIndex+delta+int(variants.size()))%int(variants.size());
        if(next==colorIndex)return;
        auto &variant=variants[next];
        bool full=armorFull||variant.broken.empty();
        if(LoadModel(full?variant.path:variant.broken)){colorIndex=next;armorFull=full;upload();}
    };
    auto fill=[&](float x,float y,float w,float h,int r,int g,int b){SDL_SetRenderDrawColor(renderer,r,g,b,255);SDL_FRect rect{x,y,w,h};SDL_RenderFillRect(renderer,&rect);};
    std::vector<SDL_Keycode> variantTest;
    if(const char *test=getenv("FMM_GALLERY_TEST");test&&std::string(test)=="all_variants"&&catalogMode){
        for(const auto &entry:catalog){
            for(const auto &variant:entry.colors){
                if(!variant.broken.empty()){variantTest.push_back(SDLK_A);variantTest.push_back(SDLK_A);}
                variantTest.push_back(SDLK_V);
            }
            variantTest.push_back(SDLK_TAB);
        }
        fprintf(stderr,"Gallery variant test: %zu steps\n",variantTest.size());
    }
    uint64_t last=SDL_GetTicks();
    while(running){
        // Integration tests pass through the same SDL event handling as real keys.
        if(const char *test=getenv("FMM_GALLERY_TEST");test&&catalogMode){
            SDL_Event key{};key.type=SDL_EVENT_KEY_DOWN;key.key.windowID=SDL_GetWindowID(window);
            if(frame<variantTest.size())key.key.key=variantTest[frame];
            else if(std::string(test)=="cycle"&&frame<catalog.size())key.key.key=SDLK_TAB;
            else if(std::string(test)=="picker"&&frame==0)key.key.key=SDLK_C;
            else if(std::string(test)=="select"&&frame==0)key.key.key=SDLK_C;
            else if(std::string(test)=="select"&&frame==1)key.key.key=SDLK_RIGHT;
            else if(std::string(test)=="select"&&frame==2)key.key.key=SDLK_RETURN;
            else if(std::string(test)=="armor"&&frame==0)key.key.key=SDLK_A;
            else if(std::string(test)=="armor_twice"&&frame<2)key.key.key=SDLK_A;
            else if(std::string(test)=="color"&&frame==0)key.key.key=SDLK_V;
            else if(std::string(test)=="color_back"&&frame<3)key.key.key=frame==2?SDLK_B:SDLK_V;
            else if(std::string(test)=="color_armor"&&frame<2)key.key.key=frame?SDLK_V:SDLK_A;
            else if(std::string(test)=="color_armor_back"&&frame<3)key.key.key=frame==1?SDLK_V:SDLK_A;
            else if(std::string(test)=="color_cycle"&&frame<3)key.key.key=SDLK_V;
            else if(std::string(test)=="hd_toggle"&&frame==0)key.key.key=SDLK_F10;
            else if(std::string(test)=="hd_twice"&&frame<2)key.key.key=SDLK_F10;
            else if(std::string(test)=="cycle_armor"&&frame<catalog.size()*2)key.key.key=frame%2?SDLK_TAB:SDLK_A;
            if(key.key.key)SDL_PushEvent(&key);
        }
        uint64_t now=SDL_GetTicks();float dt=std::min((now-last)/1000.f,0.05f);last=now;
        SDL_Event e;
        while(SDL_PollEvent(&e)){
            if(embedded){
                int w=1,h=1;SDL_GetWindowSize(window,&w,&h);
                float scale=std::max(0.001f,std::min(w/1280.f,h/720.f));
                float left=(w-1280*scale)/2,top=(h-720*scale)/2;
                if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN||e.type==SDL_EVENT_MOUSE_BUTTON_UP){e.button.x=(e.button.x-left)/scale;e.button.y=(e.button.y-top)/scale;}
                if(e.type==SDL_EVENT_MOUSE_MOTION){e.motion.xrel/=scale;e.motion.yrel/=scale;}
            }
            if(e.type==SDL_EVENT_QUIT||(e.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED&&e.window.windowID==SDL_GetWindowID(window))){running=false;if(embedded)satrecomp::TheGame().modalQuit=true;}
            if(e.type==SDL_EVENT_KEY_DOWN&&!e.key.repeat){
                if(picker){
                    if(e.key.key==SDLK_ESCAPE){picker=false;continue;}
                    if(e.key.key==SDLK_LEFT)pickCursor=(pickCursor+int(catalog.size())-1)%int(catalog.size());
                    if(e.key.key==SDLK_RIGHT)pickCursor=(pickCursor+1)%int(catalog.size());
                    if(e.key.key==SDLK_UP)pickCursor=(pickCursor+int(catalog.size())-4)%int(catalog.size());
                    if(e.key.key==SDLK_DOWN)pickCursor=(pickCursor+4)%int(catalog.size());
                    if(e.key.key==SDLK_RETURN||e.key.key==SDLK_SPACE){change(pickCursor-selected);picker=false;}
                    continue;
                }
                switch(e.key.key){
                case SDLK_ESCAPE:running=false;break;
                case SDLK_1:if(catalogMode)change(-1);else{selected=0;reset();}break;
                case SDLK_2:if(catalogMode)change(1);else{selected=1;reset();}break;
                case SDLK_TAB:change(1);break;
                case SDLK_PAGEUP:change(-1);break;
                case SDLK_PAGEDOWN:change(1);break;
                case SDLK_SPACE:spin=!spin;break;
                case SDLK_W:wire=!wire;break;
                case SDLK_R:reset();break;
                case SDLK_F10:
                    if(satrecomp::TheGame().setModelHdTextures){hd=satrecomp::TheGame().setModelHdTextures(!hd);upload();}
                    break;
                case SDLK_C:if(catalogMode){picker=true;pickCursor=selected;}break;
                case SDLK_A:armor();break;
                case SDLK_V:color(1);break;
                case SDLK_B:color(-1);break;
                default:break;
                }
            }
            if(e.type==SDL_EVENT_MOUSE_WHEEL)zoom=std::clamp(zoom*std::pow(1.12f,e.wheel.y),0.35f,4.f);
            if(e.type==SDL_EVENT_MOUSE_BUTTON_DOWN){
                if(picker){
                    int width,height;SDL_GetRenderOutputSize(renderer,&width,&height);
                    float cell=(width-80.f)/4;
                    int col=int((e.button.x-40)/cell),row=int((e.button.y-130)/66);
                    if(e.button.x>=40&&e.button.x<width-40&&e.button.y>=130&&col>=0&&col<4&&row>=0&&row<8){
                        int index=row*4+col;if(index<int(catalog.size())){change(index-selected);picker=false;}
                    }
                    continue;
                }
                if(e.button.x<360&&e.button.button==SDL_BUTTON_LEFT){
                    float y=e.button.y;
                    if(y>=655){running=false;continue;}
                    if(y>=110&&y<160&&catalogMode){picker=true;pickCursor=selected;}
                    if(y>=164&&y<218){if(catalogMode)change(-1);else{selected=0;reset();}}
                    if(y>=230&&y<284){if(catalogMode)change(1);else{selected=1;reset();}}
                    if(y>=334&&y<376)spin=!spin;
                    if(y>=386&&y<428)wire=!wire;
                    if(y>=475&&y<532)armor();
                    if(y>=575&&y<642)color(1);
                }else{drag=e.button.button==SDL_BUTTON_LEFT;pan=e.button.button==SDL_BUTTON_RIGHT;}
            }
            if(e.type==SDL_EVENT_MOUSE_BUTTON_UP){if(e.button.button==SDL_BUTTON_LEFT)drag=false;if(e.button.button==SDL_BUTTON_RIGHT)pan=false;}
            if(e.type==SDL_EVENT_MOUSE_MOTION){if(drag){yaw+=e.motion.xrel*0.009f;pitch=std::clamp(pitch+e.motion.yrel*0.009f,-1.5f,1.5f);}if(pan){panX+=e.motion.xrel;panY+=e.motion.yrel;}}
            if(e.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN){
                if(picker){
                    int b=e.gbutton.button;
                    if(b==SDL_GAMEPAD_BUTTON_EAST)picker=false;
                    if(b==SDL_GAMEPAD_BUTTON_DPAD_LEFT)pickCursor=(pickCursor+int(catalog.size())-1)%int(catalog.size());
                    if(b==SDL_GAMEPAD_BUTTON_DPAD_RIGHT)pickCursor=(pickCursor+1)%int(catalog.size());
                    if(b==SDL_GAMEPAD_BUTTON_DPAD_UP)pickCursor=(pickCursor+int(catalog.size())-4)%int(catalog.size());
                    if(b==SDL_GAMEPAD_BUTTON_DPAD_DOWN)pickCursor=(pickCursor+4)%int(catalog.size());
                    if(b==SDL_GAMEPAD_BUTTON_SOUTH){change(pickCursor-selected);picker=false;}
                    continue;
                }
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_EAST)running=false;
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER||e.gbutton.button==SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER){change(e.gbutton.button==SDL_GAMEPAD_BUTTON_LEFT_SHOULDER?-1:1);}
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_SOUTH)spin=!spin;
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_NORTH)reset();
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_WEST&&catalogMode){picker=true;pickCursor=selected;}
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_START)armor();
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_DPAD_RIGHT)color(1);
                if(e.gbutton.button==SDL_GAMEPAD_BUTTON_DPAD_LEFT)color(-1);
            }
        }
        const bool *keys=SDL_GetKeyboardState(nullptr);
        yaw+=(keys[SDL_SCANCODE_RIGHT]-keys[SDL_SCANCODE_LEFT])*dt*1.8f;
        pitch=std::clamp(pitch+(keys[SDL_SCANCODE_DOWN]-keys[SDL_SCANCODE_UP])*dt*1.5f,-1.5f,1.5f);
        zoom=std::clamp(zoom*std::exp((keys[SDL_SCANCODE_EQUALS]-keys[SDL_SCANCODE_MINUS])*dt),0.35f,4.f);
        for(auto *pad:pads){auto axis=[&](SDL_GamepadAxis a){float v=SDL_GetGamepadAxis(pad,a)/32768.f;return std::abs(v)<0.18f?0.f:v;};yaw+=axis(SDL_GAMEPAD_AXIS_RIGHTX)*dt*1.8f;pitch=std::clamp(pitch+axis(SDL_GAMEPAD_AXIS_RIGHTY)*dt*1.5f,-1.5f,1.5f);zoom=std::clamp(zoom*std::exp((axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)-axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER))*dt),0.35f,4.f);}
        if(spin)yaw+=dt*0.5f;
        int width,height;SDL_GetRenderOutputSize(renderer,&width,&height);
        float side=360,viewWidth=width-side,centerX=side+viewWidth/2+panX,centerY=height/2.f+25+panY;
        theme.Background(width,height);
        auto &model=models[catalogMode?0:selected];
        Point lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};
        for(auto &f:model.faces)for(auto &p:f.p){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);}
        Point center{(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};
        float extent=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,1.f}),scale=std::min(viewWidth*0.66f,height*0.67f)/extent*zoom;
        struct Projected {SDL_Vertex v[4];float z;unsigned texture;};std::vector<Projected> draw;
        const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
        for(auto &f:model.faces){
            Projected face{};face.texture=f.texture;
            for(int j=0;j<4;j++){
                float x=f.p[j].x-center.x,y=f.p[j].y-center.y,z=f.p[j].z-center.z;
                float xx=cy*x+sy*z,zz=-sy*x+cy*z,yy=cp*y-sp*zz;z=sp*y+cp*zz;face.z+=z;
                float perspective=extent*3/(extent*3+z);
                face.v[j].position={centerX+xx*scale*perspective,centerY+yy*scale*perspective};
                face.v[j].color={1,1,1,1};
                float u=(j==1||j==2)?1.f:0.f,v=j>=2?1.f:0.f;
                face.v[j].tex_coord={f.flipX?1-u:u,f.flipY?1-v:v};
            }
            draw.push_back(face);
        }
        std::stable_sort(draw.begin(),draw.end(),[](auto &a,auto &b){return a.z>b.z;});
        SDL_Rect clip{360,145,width-360,height-213};SDL_SetRenderClipRect(renderer,&clip);
        const int indices[]={0,1,2,0,2,3};
        for(auto &f:draw){
            if(wire){SDL_SetRenderDrawColor(renderer,255,220,80,255);for(int i=0;i<4;i++)SDL_RenderLine(renderer,f.v[i].position.x,f.v[i].position.y,f.v[(i+1)%4].position.x,f.v[(i+1)%4].position.y);}
            else SDL_RenderGeometry(renderer,gpuTextures[f.texture],f.v,4,indices,6);
        }
        SDL_SetRenderClipRect(renderer,nullptr);
        theme.Header("GALLERY",width);
        theme.Text("plain","C: SELECT CHARACTER",32,100,16,270);
        const std::string name=catalogMode?catalog[selected].name:(selected?"Player 2":"Player 1");
        theme.Text("warm",name,32,128,37,285);
        if(catalogMode){
            char counter[16];snprintf(counter,sizeof(counter),"%02d / 32",selected+1);
            theme.Text("plain",counter,290,104,15,66);
        }
        theme.Text("serif","Previous",34,181,34,270);
        theme.Text("serif","Next",34,247,34,270);
        theme.Text("plain","VIEW",32,309,18,300);
        theme.Text("serif","Auto Rotate",34,345,31,228);
        theme.Text(spin?"warm":"serif",spin?"On":"Off",282,349,26,65);
        theme.Text("serif","Wireframe",34,397,31,228);
        theme.Text(wire?"warm":"serif",wire?"On":"Off",282,401,26,65);
        theme.Text("plain","ARMOR",32,466,18,300);
        if(catalogMode&&!catalog[selected].colors[colorIndex].broken.empty()){
            theme.Text("warm",armorFull?"Full":"Broken",34,497,38,300);
            theme.Text("plain","A: CHANGE ARMOR",32,546,16,300);
        }else theme.Text("serif","Not Applicable",34,499,29,310);
        theme.Text("plain","COLOR",32,568,18,300);
        if(catalogMode&&catalog[selected].colors.size()>1){
            char label[40];snprintf(label,sizeof(label),"%d / %zu",colorIndex+1,catalog[selected].colors.size());
            theme.Text("warm",label,34,594,30,300);
            theme.Text("plain","V / B: NEXT / PREVIOUS",32,632,13,300);
        }else theme.Text("serif","Original",34,598,27,300);
        theme.Text("serif",name,side+viewWidth/2,102,39,viewWidth-90,true);
        theme.Text(hd?"warm":"serif",hd?"HD":"SD",width-90,107,28,70);
        // The same blue strip and silver rule frame the controls as the title.
        fill(0,height-65,width,65,0,0,65);
        fill(16,height-66,width-32,1,156,156,213);
        theme.Text("serif","Mode Select",32,height-52,33,300);
        theme.Text("plain","ESC: MODE SELECT",34,height-17,11,270);
        theme.Text("plain","DRAG: ROTATE     WHEEL: ZOOM     RIGHT DRAG: PAN",390,height-51,16,width-420);
        theme.Text("plain","TAB: NEXT     SPACE: AUTO ROTATE     F10: SD / HD",390,height-24,15,width-420);
        if(picker){
            theme.Background(width,height);
            theme.Header("SELECT CHARACTER",width);
            float cell=(width-80.f)/4;
            for(int i=0;i<int(catalog.size());i++){
                float x=40+(i%4)*cell,y=130+(i/4)*66;
                bool on=i==pickCursor;
                if(on)theme.Image("blade1",x-12,y+1,47,48);
                theme.Text(on?"warm":"serif",catalog[i].name,x+22,y+12,32,cell-38);
            }
            fill(0,height-65,width,65,0,0,65);
            fill(16,height-66,width-32,1,156,156,213);
            theme.Text("plain","ENTER / A: VIEW     ESC / B: BACK",width/2.f,height-44,22,width-80,true);
        }
        unsigned shotFrame=2;if(const char *given=getenv("FMM_GALLERY_SHOT_FRAME"))shotFrame=static_cast<unsigned>(std::max(0,atoi(given)));
        if(shot&&frame==shotFrame){
            SDL_Surface *surface=SDL_RenderReadPixels(renderer,nullptr);
            if(surface){SDL_Surface *rgba=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);if(rgba){std::vector<uint32_t> pixels(rgba->w*rgba->h);for(int y=0;y<rgba->h;y++)memcpy(pixels.data()+y*rgba->w,static_cast<uint8_t*>(rgba->pixels)+y*rgba->pitch,rgba->w*4);WritePng(shot,pixels.data(),rgba->w,rgba->h);SDL_DestroySurface(rgba);}SDL_DestroySurface(surface);}
            running=false;
        }
        SDL_RenderPresent(renderer);
        if(embedded&&canvas)satrecomp::TheGame().presentModal(static_cast<uint32_t*>(canvas->pixels),canvas->w,canvas->h);
        frame++;
        if(!shot)SDL_Delay(8);
    }
    for(auto *p:pads)SDL_CloseGamepad(p);
    for(auto *texture:gpuTextures)if(texture)SDL_DestroyTexture(texture);
    theme.Clear();
    SDL_DestroyRenderer(renderer);
    if(embedded){satrecomp::TheGame().presentModal(nullptr,0,0);SDL_DestroySurface(canvas);}
    else SDL_DestroyWindow(window);
    if(ownVideo)SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
}
namespace satrecomp {
bool OpenGalleryCatalog(){
    if(!LoadCatalog()){
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Gallery","Model catalog not found in the gallery folder beside the game.",nullptr);return false;
    }
    int initial=0;if(const char *index=getenv("FMM_GALLERY_INDEX"))initial=std::clamp(atoi(index),0,int(catalog.size())-1);
    // Rotate the manifest so tests can inspect any entry without simulating a match.
    if(initial)std::rotate(catalog.begin(),catalog.begin()+initial,catalog.end());
    if(!LoadModel(catalog[0].colors[0].path))return false;
    catalogMode=true;Show();catalogMode=false;return true;
}
void RegisterGallery(){
    static const Lift hooks[]={
        {"Gallery lit vertices",0x06021154,16,0x31C80FAE20A23B2Aull,Capture},
        {"Gallery vertices",0x06020a5c,16,0x31C81AAE20A24DDBull,Capture},
        {"Gallery clipped vertices",0x06020adc,16,0x31C81BAE20A24F8Eull,Capture},
        {"Gallery gouraud vertices",0x060213e0,16,0xC4D851C1A88E4094ull,Capture},
        {"Gallery extra vertices",0x06021768,16,0x31C81AAE20A24DDBull,Capture},
    };
    RegisterLifts(hooks,std::size(hooks));
    RegisterModeSelectGallery();
    if(const char *capture=getenv("FMM_GALLERY_CAPTURE");capture&&*capture)Request();
}
void GalleryAfterFrame(){
    ModeSelectGalleryAfterFrame();
    static uint64_t preparationFrame=0;preparationFrame++;
    if(const char *at=getenv("FMM_GALLERY_CAPTURE_FRAME");at&&*at&&preparationFrame==strtoull(at,nullptr,10))Request();
    if(!requested)return;
    if(++captureFrames<3){for(auto &model:models)model.faces.clear();return;}
    capturing=requested=false;Dump();Show();
}
}
