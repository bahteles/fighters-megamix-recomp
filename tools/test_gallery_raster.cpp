// SPDX-License-Identifier: GPL-3.0-only
// Regression checks for Gallery depth and transparent texels; no game resources.
#include "gallery_raster.h"
#include <cassert>
#include <limits>
using namespace gallery;
int main(){
 uint32_t red=0xff0000ff,blue=0xffff0000,transparent=0;
 Image redImage{1,1,&red,false},blueImage{1,1,&blue,false},clearImage{1,1,&transparent,false};
 Vertex a{1,1,1,0,0},b{14,1,12,1,0},c{1,14,12,0,1};
 Vertex d=a,e=b,f=c;d.depth=e.depth=f.depth=4;
 auto render=[&](bool reverse){
  std::vector<uint32_t> pixels(256);std::vector<float> depth(256,std::numeric_limits<float>::infinity());
  if(reverse)Triangle(pixels,depth,16,16,0,0,16,16,d,e,f,blueImage);
  Triangle(pixels,depth,16,16,0,0,16,16,a,b,c,redImage);
  if(!reverse)Triangle(pixels,depth,16,16,0,0,16,16,d,e,f,blueImage);
  assert(pixels[2*16+2]==red);assert(pixels[2*16+11]==blue);
  Vertex x=d,y=e,z=f;x.depth=y.depth=z.depth=.5f;
  auto before=pixels;Triangle(pixels,depth,16,16,0,0,16,16,x,y,z,clearImage);assert(pixels==before);
  return pixels;
 };
 assert(render(false)==render(true));
 uint32_t edge[]={0x00000000,0xffffffff};Image smooth{2,1,edge,true};
 uint32_t sample=Sample(smooth,.5f,.5f);assert((sample&0xffffff)==0xffffff&&(sample>>24)>0);
}
