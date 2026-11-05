// SPDX-License-Identifier: GPL-3.0-only
// Standalone geometry interpolation checks; no game resources required.
#include "interpolation.h"
#include <cassert>
using namespace satrecomp::video;
Quad quad(float x,unsigned texture=16) {Quad q{};q.texture=texture;q.width=8;q.height=8;q.corner[0][0]=q.corner[3][0]=x;q.corner[1][0]=q.corner[2][0]=x+8;q.corner[2][1]=q.corner[3][1]=8;return q;}
int main(){
 std::vector<Quad> old{quad(0)},now{quad(10)};
 assert(InterpolateQuads(old,now,.5f)==1&&now[0].corner[0][0]==5);
 now={quad(10)};InterpolateQuads(old,now,0);assert(now[0].corner[0][0]==0);
 now={quad(10)};InterpolateQuads(old,now,1);assert(now[0].corner[0][0]==10);
 now={quad(100)};assert(InterpolateQuads(old,now,.5f)==0&&now[0].corner[0][0]==100);
 now={quad(10,32)};assert(InterpolateQuads(old,now,.5f)==0);
 old={quad(0),quad(2)};now={quad(1)};assert(InterpolateQuads(old,now,.5f)==0);
 old={quad(0,16),quad(20,32)};now={quad(24,32),quad(10,16),quad(9,48)};
 assert(InterpolateQuads(old,now,.5f)==2&&now[0].corner[0][0]==22&&now[1].corner[0][0]==5&&now[2].corner[0][0]==9);
 old={quad(0)};now={quad(10)};now[0].flags=kQuadErase;assert(InterpolateQuads(old,now,.5f)==0);
 now={quad(10)};now[0].clip[0]=1;assert(InterpolateQuads(old,now,.5f)==0);
}
