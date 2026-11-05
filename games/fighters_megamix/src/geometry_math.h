// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cmath>
#include <cstdint>
namespace geometry {
struct Transform {double precise[3];int original[3];};
inline Transform Apply(const int32_t *matrix,const int32_t *point){
    Transform result{};
    for(int row=0;row<3;row++){
        double sum=0;for(int a=0;a<3;a++)sum+=double(matrix[row*4+a])*point[a];
        // The original transform discards the dot product's fractional bits and
        // reads only the signed integer half of the translation. Preserve both here.
        result.original[row]=int(std::floor(sum/4294967296.0))+int16_t(uint32_t(matrix[row*4+3])>>16);
        result.precise[row]=sum/4294967296.0+double(matrix[row*4+3])/65536.0;
    }
    return result;
}
inline int OriginalProjection(int coordinate,int32_t scale){
    return int16_t(int64_t(std::floor(double(coordinate)*scale/65536.0))&0xffff);
}
} // namespace geometry
