// SPDX-License-Identifier: GPL-3.0-only
// Observe the original vertex transforms; keep subpixel coordinates on the host only.
#include "geometry.h"
#include "geometry_math.h"
#include "geometry_key.h"
#include "game.h"
#include "lift.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <mutex>

namespace satrecomp {
namespace {
struct Position {double x,y; int ix,iy; bool valid=false;};
using Key=geometry::Key;
struct Entry {std::array<float,8> xy;unsigned epoch;bool ambiguous=false;};
std::unordered_map<Key,Entry,geometry::KeyHash> cache;
std::mutex cacheMutex;
unsigned epoch=0;unsigned long long queries=0,hits=0;
bool Valid(uint32_t at,uint32_t size){return at>=0x06000000&&at<=0x06100000-size;}
int Signed(uint32_t value){return static_cast<int32_t>(value);}
Key MakeKey(const int (*points)[2]){Key key{};for(int i=0;i<4;i++){key[2*i]=points[i][0];key[2*i+1]=points[i][1];}return key;}
bool Correct(const int (*points)[2],const uint16_t *command,float (*precise)[2]){
    if(!FeatureOn("geometry_correction"))return false;
    std::lock_guard lock(cacheMutex);++queries;
    Key key=MakeKey(points);
    key[8]=(command[0]&15)==4?1:0;
    key[9]=key[8]?0:command[4]&(((command[2]>>3)&7)==5?~1:~0);
    key[10]=key[8]?0:command[5];
    std::array<int,4> order;
    key=geometry::Canonical(key,order);
    auto it=cache.find(key);
    if(it==cache.end()||it->second.ambiguous||epoch-it->second.epoch>2)return false;
    for(int i=0;i<4;i++)for(int a=0;a<2;a++)precise[order[i]][a]=it->second.xy[2*i+a];
    ++hits;return true;
}
}
void ResetGeometry(){std::lock_guard lock(cacheMutex);cache.clear();++epoch;}
void GeometryAfterFrame(){
    std::lock_guard lock(cacheMutex);++epoch;
    for(auto it=cache.begin();it!=cache.end();)if(epoch-it->second.epoch>2)it=cache.erase(it);else ++it;
    if(getenv("FMM_GEOMETRY_LOG")&&epoch%60==0)fprintf(stderr,"Geometry precision: %llu/%llu quads, %zu cached\n",hits,queries,cache.size());
}
void ObserveGeometry(Guest &g){
    if(!FeatureOn("geometry_correction"))return;
    const uint32_t data=g.R(4),matrix=g.R(6);
    if(!Valid(data,20)||!Valid(matrix,48))return;
    const uint32_t points=g.Read32(data),count=g.Read32(data+4),faces=g.Read32(data+8),faceCount=g.Read32(data+12),attributes=g.Read32(data+16);
    if(count<3||count>1024||!faceCount||faceCount>2048||!Valid(points,count*12)||!Valid(faces,faceCount*20)||!Valid(attributes,faceCount*12))return;
    int32_t m[12];for(int i=0;i<12;i++)m[i]=Signed(g.Read32(matrix+i*4));
    std::vector<Position> projected(count);
    for(unsigned i=0;i<count;i++){
        int32_t point[3];for(int a=0;a<3;a++)point[a]=Signed(g.Read32(points+i*12+a*4));
        const auto transform=geometry::Apply(m,point);
        const auto &transformed=transform.precise;
        const auto &original=transform.original;
        int z=original[2];if(z<32||z>16000||transformed[2]<32||transformed[2]>16000)continue;
        auto index=[](int depth){return uint32_t(depth>=4096?depth/4+0x1c00:depth*2)&~7u;};
        const uint32_t table=0x0607ac28;
        double factor[2];int exact[2];
        const int bucket=transformed[2]>=4096?16:4;
        int lower=int(std::floor(transformed[2]/bucket))*bucket;
        double t=(transformed[2]-lower)/bucket;
        for(int a=0;a<2;a++){
            int32_t oldScale=Signed(g.Read32(table+index(z)+a*4));
            double first=Signed(g.Read32(table+index(lower)+a*4))/65536.0;
            double second=Signed(g.Read32(table+index(lower+bucket)+a*4))/65536.0;
            factor[a]=first+(second-first)*t;
            exact[a]=geometry::OriginalProjection(original[a],oldScale);
        }
        auto &p=projected[i];p.x=transformed[0]*factor[0];p.y=transformed[1]*factor[1];p.ix=exact[0];p.iy=exact[1];
        p.valid=std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x-p.ix)<2.5&&std::abs(p.y-p.iy)<2.5&&std::abs(p.ix)<2048&&std::abs(p.iy)<2048;
    }
    std::lock_guard lock(cacheMutex);
    for(unsigned face=0;face<faceCount;face++){
        unsigned ids[4];bool valid=true;
        for(int c=0;c<4;c++){ids[c]=g.Read16(faces+face*20+12+c*2)/16;if(ids[c]>=count||!projected[ids[c]].valid)valid=false;}
        if(!valid)continue;
        const uint32_t attr=attributes+face*12;
        const uint16_t mode=g.Read16(attr),flags=g.Read16(attr+8);
        const int flat=(flags&7)==4?1:0;
        const int source=flat?0:g.Read16(attr+4)&(((mode>>3)&7)==5?~1:~0);
        const int size=flat?0:g.Read16(attr+6);
        Key raw{};Entry entry{};entry.epoch=epoch;
        for(int c=0;c<4;c++){const auto &p=projected[ids[c]];raw[2*c]=p.ix;raw[2*c+1]=p.iy;}
        raw[8]=flat;raw[9]=source;raw[10]=size;
        std::array<int,4> order;Key key=geometry::Canonical(raw,order);
        for(int c=0;c<4;c++){
            const auto &p=projected[ids[order[c]]];entry.xy[2*c]=float(p.x);entry.xy[2*c+1]=float(p.y);

        }
        // Reject only genuinely ambiguous full-face correspondences, not ordinary
        // triangular faces whose final corner repeats the previous vertex.
        for(int reverse=0;reverse<2;reverse++)for(int start=0;start<4;start++){
            bool same=true;
            for(int c=0;c<4;c++){int i=(start+(reverse?4-c:c))%4;if(raw[2*i]!=key[2*c]||raw[2*i+1]!=key[2*c+1])same=false;}
            if(same)for(int c=0;c<4;c++){
                const auto &p=projected[ids[(start+(reverse?4-c:c))%4]];
                if(std::abs(float(p.x)-entry.xy[2*c])>.125f||std::abs(float(p.y)-entry.xy[2*c+1])>.125f)entry.ambiguous=true;
            }
        }
        auto it=cache.find(key);
        if(it!=cache.end()&&it->second.epoch==epoch){
            for(int c=0;c<8;c++)if(std::abs(it->second.xy[c]-entry.xy[c])>.125f)entry.ambiguous=true;
            entry.ambiguous|=it->second.ambiguous;
        }
        cache[key]=entry;
    }
}
void RegisterGeometry(){
    TheGame().features.push_back({"geometry_correction","Display","Geometry correction",
        "Reduce polygon wobble using higher-precision vertex positions. Requires high resolution. Unmatched geometry keeps its original coordinates."});
    TheGame().preciseQuad=Correct;
}
} // namespace satrecomp
