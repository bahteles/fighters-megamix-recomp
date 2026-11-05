// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "frame.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <tuple>

namespace satrecomp::video {
// Match source commands rather than blending finished images. This tolerates culling and
// painter-order changes while rejecting ambiguous matches, large jumps and new geometry.
inline size_t InterpolateQuads(const std::vector<Quad> &previous, std::vector<Quad> &current, float alpha) {
    using Key = std::tuple<uint32_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t>;
    auto key=[](const Quad &q){return Key{q.texture,q.width,q.height,q.color,q.mode,q.flags};};
    std::map<Key,std::vector<size_t>> groups;
    for (size_t i=0;i<previous.size();++i) if (!(previous[i].flags & kQuadErase)) groups[key(previous[i])].push_back(i);
    std::vector<bool> used(previous.size());
    size_t moved=0;
    alpha=std::clamp(alpha,0.f,1.f);
    for (auto &q:current) {
        if (q.flags & kQuadErase) continue;
        auto group=groups.find(key(q)); if (group==groups.end()) continue;
        float best=std::numeric_limits<float>::infinity(),second=best;
        size_t chosen=previous.size();
        for (size_t i:group->second) {
            if (used[i]) continue;
            const auto &old=previous[i];
            if (std::memcmp(q.clip,old.clip,sizeof(q.clip)) || std::memcmp(q.clipOut,old.clipOut,sizeof(q.clipOut))) continue;
            float distance=0,maximum=0;
            for (int c=0;c<4;++c) for(int axis=0;axis<2;++axis) {
                float delta=q.corner[c][axis]-old.corner[c][axis];
                maximum=std::max(maximum,std::abs(delta));distance+=delta*delta;
            }
            if (!std::isfinite(distance) || maximum>32) continue;
            if (distance<best) {second=best;best=distance;chosen=i;} else second=std::min(second,distance);
        }
        if (chosen==previous.size() || second<=best*1.5f+0.5f) continue;
        used[chosen]=true;
        if (best==0) continue;
        ++moved;
        for(int c=0;c<4;++c) for(int axis=0;axis<2;++axis)
            q.corner[c][axis]=previous[chosen].corner[c][axis]+alpha*(q.corner[c][axis]-previous[chosen].corner[c][axis]);
    }
    return moved;
}
} // namespace satrecomp::video
