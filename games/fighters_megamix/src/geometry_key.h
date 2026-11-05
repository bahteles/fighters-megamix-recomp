// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace geometry {
using Key=std::array<int,11>;
// A face may start at any corner and use either winding. Store one canonical
// record, rather than eight copies, and retain the inverse corner correspondence.
inline Key Canonical(const Key &input,std::array<int,4> &order){
    Key best{};bool first=true;
    for(int reverse=0;reverse<2;reverse++)for(int start=0;start<4;start++){
        Key candidate=input;std::array<int,4> indices{};
        for(int c=0;c<4;c++){
            int i=(start+(reverse?4-c:c))%4;indices[c]=i;
            candidate[c*2]=input[i*2];candidate[c*2+1]=input[i*2+1];
        }
        if(first||candidate<best){best=candidate;order=indices;first=false;}
    }
    return best;
}
struct KeyHash {size_t operator()(const Key &key)const{
    uint64_t hash=14695981039346656037ull;
    for(int value:key){hash^=uint32_t(value);hash*=1099511628211ull;}
    return size_t(hash);
}};
} // namespace geometry
