// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gallery {
struct Vertex {float x,y,depth,u,v;};
struct Image {int width,height;const uint32_t *pixels;bool smooth;};
inline uint32_t Sample(const Image &image,float u,float v) {
    if(!image.smooth) {
        int x=std::clamp(int(u*image.width),0,image.width-1),y=std::clamp(int(v*image.height),0,image.height-1);
        return image.pixels[y*image.width+x];
    }
    float x=std::clamp(u*image.width-.5f,0.f,float(image.width-1));
    float y=std::clamp(v*image.height-.5f,0.f,float(image.height-1));
    int x0=int(x),y0=int(y),x1=std::min(x0+1,image.width-1),y1=std::min(y0+1,image.height-1);
    float fx=x-x0,fy=y-y0,weights[4]={(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
    uint32_t pixels[4]={image.pixels[y0*image.width+x0],image.pixels[y0*image.width+x1],image.pixels[y1*image.width+x0],image.pixels[y1*image.width+x1]};
    float alpha=0,channels[3]{};
    for(int i=0;i<4;i++) {
        float a=float(pixels[i]>>24)*weights[i];alpha+=a;
        for(int c=0;c<3;c++)channels[c]+=float((pixels[i]>>(8*c))&255)*a;
    }
    if(alpha<1)return 0;
    uint32_t result=uint32_t(std::clamp(std::lround(alpha),0l,255l))<<24;
    for(int c=0;c<3;c++)result|=uint32_t(std::clamp(std::lround(channels[c]/alpha),0l,255l))<<(8*c);
    return result;
}
// Resolve visibility per pixel, independent of command order. Reciprocal depth also
// provides perspective-correct UVs; transparent texels must not hide faces behind them.
inline void Triangle(std::vector<uint32_t> &pixels,std::vector<float> &depth,int width,int height,
                     int left,int top,int right,int bottom,const Vertex &a,const Vertex &b,const Vertex &c,const Image &image) {
    auto edge=[](const Vertex &p,const Vertex &q,float x,float y){return (q.x-p.x)*(y-p.y)-(q.y-p.y)*(x-p.x);};
    float area=edge(a,b,c.x,c.y);
    if(!std::isfinite(area)||std::abs(area)<.0001f||a.depth<=0||b.depth<=0||c.depth<=0)return;
    int x0=std::max(left,int(std::floor(std::min({a.x,b.x,c.x})))),x1=std::min(right-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
    int y0=std::max(top,int(std::floor(std::min({a.y,b.y,c.y})))),y1=std::min(bottom-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
    x0=std::max(0,x0);y0=std::max(0,y0);x1=std::min(width-1,x1);y1=std::min(height-1,y1);
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++) {
        float wa=edge(b,c,x+.5f,y+.5f)/area,wb=edge(c,a,x+.5f,y+.5f)/area,wc=1-wa-wb;
        if(wa<-.00001f||wb<-.00001f||wc<-.00001f)continue;
        float reciprocal=wa/a.depth+wb/b.depth+wc/c.depth;
        float z=1/reciprocal;size_t at=size_t(y)*width+x;
        if(z>=depth[at])continue;
        float u=(wa*a.u/a.depth+wb*b.u/b.depth+wc*c.u/c.depth)*z;
        float v=(wa*a.v/a.depth+wb*b.v/b.depth+wc*c.v/c.depth)*z;
        uint32_t color=Sample(image,u,v);
        if(!(color>>24))continue;
        pixels[at]=color;depth[at]=z;
    }
}
} // namespace gallery
