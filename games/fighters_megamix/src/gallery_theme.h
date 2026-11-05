// SPDX-License-Identifier: GPL-3.0-only
// Local artwork derived from the player's original menu and installed HD font set.
#pragma once
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

class GalleryTheme {
    struct Art { SDL_Texture *texture=nullptr; uint32_t width=0,height=0; };
    SDL_Renderer *renderer;
    std::unordered_map<std::string,Art> images;
public:
    explicit GalleryTheme(SDL_Renderer *r):renderer(r){}
    ~GalleryTheme(){Clear();}
    void Clear(){for(auto &[name,art]:images)SDL_DestroyTexture(art.texture);images.clear();}
    bool Load(){
        std::ifstream file(std::filesystem::path(SDL_GetBasePath())/"gallery/gallery-theme.fmui",std::ios::binary);
        auto number=[&]{uint32_t value=0;file.read(reinterpret_cast<char*>(&value),4);return value;};
        char magic[8];file.read(magic,8);
        if(!file||std::string(magic,8)!="FMMGUI02")return false;
        uint32_t count=number();if(count>512)return false;
        for(uint32_t i=0;i<count;i++){
            uint32_t length=number(),width=number(),height=number();
            if(!file||!length||length>256||!width||width>4096||!height||height>1024)return false;
            std::string name(length,'\0');file.read(name.data(),length);
            std::vector<uint32_t> pixels(size_t(width)*height);
            file.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);if(!file)return false;
            SDL_Texture *texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,width,height);
            if(!texture)return false;
            SDL_UpdateTexture(texture,nullptr,pixels.data(),width*4);
            SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);
            images.emplace(name,Art{texture,width,height});
        }
        return images.contains("background")&&images.contains("header");
    }
    void Image(const std::string &name,float x,float y,float width,float height){
        auto it=images.find(name);if(it==images.end())return;
        SDL_FRect rect{x,y,width,height};SDL_RenderTexture(renderer,it->second.texture,nullptr,&rect);
    }
    void Text(const std::string &style,const std::string &value,float x,float y,float height,float maxWidth=1200,bool centered=false){
        auto it=images.find(style+":"+value);if(it==images.end())return;
        const auto &art=it->second;
        float width=height*art.width/art.height;
        if(width>maxWidth){height*=maxWidth/width;width=maxWidth;}
        Image(style+":"+value,centered?x-width/2:x,y,width,height);
    }
    void Background(int width,int height,bool light=false){Image(light?"background-light":"background",0,0,float(width),float(height));}
    void Header(const std::string &title,int width){
        Image("header",0,0,float(width),79);
        Text("plain",title,width/2.f,20,43,width-80,true);
    }
};
