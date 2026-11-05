// SPDX-License-Identifier: GPL-3.0-only
#include "geometry_math.h"
#include <cassert>
#include <cmath>
int main(){
 int32_t matrix[12]={65536,0,0,0,0,65536,0,0,0,0,65536,0};
 int32_t a[3]={0,0,100*65536},b[3]={8192,0,100*65536},c[3]={16384,0,100*65536};
 auto x=geometry::Apply(matrix,a),y=geometry::Apply(matrix,b),z=geometry::Apply(matrix,c);
 assert(x.original[0]==y.original[0]&&y.original[0]==z.original[0]);
 assert(x.precise[0]==0&&y.precise[0]==.125&&z.precise[0]==.25);
 matrix[3]=-32768;auto negative=geometry::Apply(matrix,c);
 assert(negative.original[0]==-1&&negative.precise[0]==-.25);
 assert(geometry::OriginalProjection(-1,32768)==-1);
 assert(geometry::OriginalProjection(3,32768)==1);
 matrix[0]=32768;matrix[3]=0;auto scaled=geometry::Apply(matrix,c);assert(scaled.precise[0]==.125);
}
