// SPDX-License-Identifier: GPL-3.0-only
#include "geometry_key.h"
#include <cassert>
int main(){
 geometry::Key key{4,7,10,8,11,13,3,12,0,24,0x808};std::array<int,4> baseOrder;
 auto canonical=geometry::Canonical(key,baseOrder);
 for(int reverse=0;reverse<2;reverse++)for(int start=0;start<4;start++){
  auto rotated=key;
  for(int c=0;c<4;c++){int i=(start+(reverse?4-c:c))%4;rotated[2*c]=key[2*i];rotated[2*c+1]=key[2*i+1];}
  std::array<int,4> order;auto got=geometry::Canonical(rotated,order);assert(got==canonical);
  for(int c=0;c<4;c++)for(int a=0;a<2;a++)assert(got[2*c+a]==rotated[2*order[c]+a]);
 }
 auto other=key;other[9]++;std::array<int,4> order;assert(geometry::Canonical(other,order)!=canonical);
 geometry::Key triangle{1,1,8,1,1,8,1,8,1,0,0};auto t=geometry::Canonical(triangle,order);assert(t[8]==1);
}
