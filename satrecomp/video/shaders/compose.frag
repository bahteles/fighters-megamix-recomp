#version 450
// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of satrecomp: https://github.com/tmcleroy/satrecomp
// The picture: what the VDP2 makes of the sprite framebuffer and its own layers. For every pixel the layer
// with the highest priority that has something there shows (between equals: sprites, RBG0, NBG0..NBG3); where
// none has, the back screen. Sprite shadows darken what is below them.
layout(location = 0) in vec2 uv;

layout(set = 2, binding = 0) uniform usampler2D sprites; // the VDP1 framebuffer on display
layout(set = 2, binding = 1) uniform sampler2D colors;   // colour RAM, 2048 colours in a row
layout(set = 2, binding = 2) uniform sampler2D rbg0;     // layers: colour, priority in alpha (0: nothing)
layout(set = 2, binding = 3) uniform sampler2D nbg0;
layout(set = 2, binding = 4) uniform sampler2D nbg1;
layout(set = 2, binding = 5) uniform sampler2D nbg2;
layout(set = 2, binding = 6) uniform sampler2D nbg3;
layout(set = 2, binding = 7) uniform sampler2D window;   // where the sprite layer is cut out
layout(set = 2, binding = 8) uniform sampler2D back;     // per row: back screen colour; alpha 0: display off
layout(set = 2, binding = 9) uniform usampler2D vram2;   // VDP2 video memory, 1024 bytes a row
layout(set = 2, binding = 10) uniform sampler2D floorLines; // the rotation screen, per line: [0] where its first
                                                            // and last dots look in the map, [1].x what the line is

layout(set = 2, binding = 11) uniform usampler2D hdAtlas;

layout(set = 2, binding = 12) uniform sampler2D backgroundLines;

layout(set = 3, binding = 0, std140) uniform FragmentUniforms {
    vec4 size;      // xy: the VDP2 picture's size; zw: the sprite target's size
    ivec4 sprite;   // sprite type, mixed RGB/palette, flags (1 window used, 2 enabled, 4 inverted, 8 sprites on), colour offset
    ivec4 priLow;   // the sprite priority registers
    ivec4 priHigh;
    ivec4 misc;     // x: which layers take sprite shadows (bit 1 RBG0 .. bit 5 NBG3, bit 6 back); y: samples a side;
                    // z: columns at each side of the picture that show black (widescreen, in a scene without)
    ivec4 hdText[32]; // left, top, width, height of replacement graphics
    ivec4 hdTextMeta[8]; // packed atlas offset, scale and layer
    vec4 colorOffset[7]; // added to the colour of the layer shown (sprites, RBG0, NBG0..3, back), for fades
    ivec4 floorSize;     // the rotation screen: lines of a field, dots a line, flags, priority
    ivec4 floorMore;     // its colour offset, the extra bits of its character numbers and palettes, memory access
    uvec4 floorPlanes[4]; // the addresses of the 4x4 planes of its map
    uvec4 hdAirport;    // offset of the optional airport map in the replacement atlas
    ivec4 menuBackground; // 1 title gray, 2 repeating menu checkerboard
    uvec4 hdBackground;
};

layout(location = 0) out vec4 out_color;

// How each of the 16 sprite data formats packs colour and priority: colour bits, priority shift, priority
// bits, and whether bit 15 is the shadow / window bit
const ivec4 kFormats[16] = ivec4[16](
    ivec4(11, 14, 2, 0), ivec4(11, 13, 3, 0), ivec4(11, 14, 1, 1), ivec4(11, 13, 2, 1),
    ivec4(10, 13, 2, 1), ivec4(11, 12, 3, 1), ivec4(10, 12, 3, 1), ivec4(9, 12, 3, 1),
    ivec4(7, 7, 1, 0),   ivec4(6, 7, 1, 0),   ivec4(6, 6, 2, 0),   ivec4(6, 0, 0, 0),
    ivec4(8, 7, 1, 0),   ivec4(8, 7, 1, 0),   ivec4(8, 6, 2, 0),   ivec4(8, 0, 0, 0));

int spritePriority(int index) {
    return index < 4 ? priLow[index] : priHigh[index - 4];
}

uint vram2Byte(uint address) {
    return texelFetch(vram2, ivec2(int(address & 1023u), int((address >> 10) & 511u)), 0).r;
}

// The rotation screen at `at`, read from its map in video memory: a plane seen in perspective, each line of
// the screen a straight line across the map. Lines are given for the original's resolution; between two of
// them the positions are taken in between, so the plane is drawn as finely as the picture is. Returns the
// colour, and the priority in alpha (0: nothing); negative alpha: this line is in the layer's image instead.
vec4 floorLayer(vec2 at) {
    int lines = floorSize.x;
    float line = at.y * float(lines) - 0.5;
    int l0 = clamp(int(floor(line)), 0, lines - 1), l1 = min(l0 + 1, lines - 1);
    float kind0 = texelFetch(floorLines, ivec2(1, l0), 0).x, kind1 = texelFetch(floorLines, ivec2(1, l1), 0).x;
    int nearest = clamp(int(floor(at.y * float(lines))), 0, lines - 1);
    float kind = texelFetch(floorLines, ivec2(1, nearest), 0).x;
    if (kind > 1.5) {
        return vec4(0.0, 0.0, 0.0, -1.0);
    }
    if (kind < 0.5) {
        return vec4(0.0);
    }
    vec4 ends;
    if (kind0 > 0.5 && kind0 < 1.5 && kind1 > 0.5 && kind1 < 1.5) {
        ends = mix(texelFetch(floorLines, ivec2(0, l0), 0), texelFetch(floorLines, ivec2(0, l1), 0), clamp(line - float(l0), 0.0, 1.0));
    } else {
        ends = texelFetch(floorLines, ivec2(0, nearest), 0);
    }
    float dots = float(floorSize.y);
    vec2 where = mix(ends.xy, ends.zw, (at.x * dots - 0.5) / max(dots - 1.0, 1.0));
    ivec2 m = ivec2(floor(where));

    int flags = floorSize.z;
    uint fourCell = uint(flags & 1);
    bool twoWord = (flags & 2) != 0, extended = (flags & 4) != 0, wide = (flags & 8) != 0;
    int over = (flags >> 8) & 3;
    int pageShiftH = (flags >> 12) & 1, pageShiftV = (flags >> 13) & 1;
    if (over != 0) {
        // (outside the map there is nothing)
        ivec2 size = over == 2 ? ivec2(512) : ivec2(2048 << pageShiftH, 2048 << pageShiftV);
        if (m.x < 0 || m.y < 0 || m.x >= size.x || m.y >= size.y) {
            return vec4(0.0);
        }
    }
    uint x = uint(m.x), y = uint(m.y);
    uint plane = ((x >> (9 + pageShiftH)) & 3u) + (((y >> (9 + pageShiftV)) & 3u) << 2);
    uint planeAddress = floorPlanes[plane >> 2][plane & 3u];
    uint page = (((x >> 9) & 1u) & uint(pageShiftH)) + ((((y >> 9) & 1u) & uint(pageShiftV)) << 1);
    uint charIndex = (((x >> 3) & 63u) >> fourCell) + ((((y >> 3) & 63u) >> fourCell) << (6u - fourCell));
    uint pageShift = (fourCell != 0u ? 11u : 13u) + (twoWord ? 1u : 0u);
    uint pageAddress = planeAddress + (page << pageShift);
    uint access = uint(floorMore.w);

    // The pattern name: which character, its palette and flips
    uint number = 0u, palette = 0u;
    bool flipH = false, flipV = false, special = false;
    if (twoWord) {
        uint address = pageAddress + charIndex * 4u;
        if ((access & (1u << ((address >> 17) & 3u))) != 0u) {
            uint hi = (vram2Byte(address) << 8) | vram2Byte(address + 1u), lo = (vram2Byte(address + 2u) << 8) | vram2Byte(address + 3u);
            number = lo & 0x7FFFu;
            palette = hi & 0x7Fu;
            special = (hi & 0x2000u) != 0u;
            flipH = (hi & 0x4000u) != 0u;
            flipV = (hi & 0x8000u) != 0u;
        }
    } else {
        uint address = pageAddress + charIndex * 2u;
        if ((access & (1u << ((address >> 17) & 3u))) != 0u) {
            uint word = (vram2Byte(address) << 8) | vram2Byte(address + 1u);
            uint more = uint(floorMore.y);
            uint numberBits = extended ? 12u : 10u;
            uint moreStart = 2u * fourCell + (extended ? 2u : 0u);
            number = (word & ((1u << numberBits) - 1u)) << (2u * fourCell);
            number |= ((more & 0x1Fu) >> moreStart) << (10u + moreStart);
            if (fourCell != 0u) {
                number |= more & 3u;
            }
            number &= 0x7FFFu;
            palette = wide ? ((word >> 12) & 7u) << 4 : (((word >> 12) & 0xFu) | uint(floorMore.z));
            palette &= 0x7Fu;
            special = (flags & 64) != 0;
            flipH = !extended && (word & 0x400u) != 0u;
            flipV = !extended && (word & 0x800u) != 0u;
        }
    }
    uint cell = ((x >> 3) & fourCell) + (((y >> 3) & fourCell) << 1);
    uint dotX = x & 7u, dotY = y & 7u;
    if (flipH) {
        dotX ^= 7u;
        cell ^= fourCell;
    }
    if (flipV) {
        dotY ^= 7u;
        cell ^= fourCell << 1;
    }
    uint row = ((number + (wide ? cell << 1 : cell)) << 5) + dotY * (wide ? 8u : 4u);
    uint dot = 0u;
    if ((access & (16u << ((row >> 17) & 3u))) != 0u) {
        dot = wide ? vram2Byte(row + dotX) : (vram2Byte(row + (dotX >> 1)) >> ((~dotX & 1u) * 4u)) & 0xFu;
    }
    if (dot == 0u && (flags & 16) != 0) {
        return vec4(0.0);
    }
    uint index = uint(floorMore.x) + (wide ? (((palette << 4) & 0x700u) | dot) : ((palette << 4) | dot));
    int priority = floorSize.w;
    if ((flags & 32) != 0) {
        priority = (priority & ~1) | (special ? 1 : 0);
    }
    return vec4(texelFetch(colors, ivec2(int(index & 2047u), 0), 0).rgb, float(priority) / 255.0);
}

uint hdByte(uint address) {
    return texelFetch(hdAtlas, ivec2(int(address & 16383u), int(address >> 14)), 0).r;
}

vec3 airportFloor(vec2 at, vec3 original) {
    if (hdAirport.x == 0u) {
        return original;
    }
    float line = at.y * float(floorSize.x) - 0.5;
    int nearest = clamp(int(floor(line + 0.5)), 0, floorSize.x - 1);
    if (texelFetch(floorLines, ivec2(1, nearest), 0).x != 3.0) {
        return original;
    }
    int l0 = clamp(int(floor(line)), 0, floorSize.x - 1);
    int l1 = min(l0 + 1, floorSize.x - 1);
    vec4 ends = texelFetch(floorLines, ivec2(0, nearest), 0);
    if (texelFetch(floorLines, ivec2(1, l0), 0).x == 3.0 &&
        texelFetch(floorLines, ivec2(1, l1), 0).x == 3.0) {
        ends = mix(texelFetch(floorLines, ivec2(0, l0), 0), texelFetch(floorLines, ivec2(0, l1), 0), clamp(line - float(l0), 0.0, 1.0));
    }
    float dots = float(floorSize.y);
    vec2 where = mix(ends.xy, ends.zw, (at.x * dots - 0.5) / max(dots - 1.0, 1.0));
    // Outside the mapped airport apron the game repeats its asphalt character.
    // Continue the matching outer asphalt replacement through that region too.
    uint address;
    if (all(greaterThanEqual(where, vec2(512.0))) && all(lessThan(where, vec2(1536.0)))) {
        uvec2 pixel = uvec2((where - vec2(512.0)) * 2.0);
        address = hdAirport.x + 3u * (pixel.y * 2048u + pixel.x);
    } else {
        uvec2 pixel = uvec2(mod(where, 512.0) * 2.0);
        address = hdAirport.x + 3u * (2048u * 2048u + pixel.y * 1024u + pixel.x);
    }
    return vec3(hdByte(address), hdByte(address + 1u), hdByte(address + 2u)) / 255.0;
}

vec4 backgroundLayer(vec4 original, int layer, vec2 at) {
    if (layer > 1 || hdBackground[layer] == 0u || original.a == 0.0) { return original; }
    float row = at.y * size.y - 0.5;
    int nearest = clamp(int(floor(row + 0.5)), 0, int(size.y) - 1);
    vec4 coordinates = texelFetch(backgroundLines, ivec2(layer, nearest), 0);
    if (coordinates.w < 0.5) { return original; }
    int next = min(nearest + 1, int(size.y) - 1);
    vec4 following = texelFetch(backgroundLines, ivec2(layer, next), 0);
    float dy = mod(following.y - coordinates.y + 512.0, 1024.0) - 512.0;
    float y = coordinates.y + dy * (row - float(nearest));
    vec2 where = mod(vec2(coordinates.x + (at.x * size.x - 0.5) * coordinates.z, y), 1024.0);
    // The distant panorama uses the same page in both horizontal planes.
    if (layer == 1) { where.x = mod(where.x, 512.0); }
    if (any(greaterThanEqual(where, vec2(512.0)))) { return original; }
    uvec2 pixel = uvec2(where * 2.0);
    uint address = hdBackground[layer] + 4u * (pixel.y * 1024u + pixel.x);
    if (hdByte(address + 3u) == 0u) { return original; }
    return vec4(vec3(hdByte(address), hdByte(address + 1u), hdByte(address + 2u)) / 255.0, original.a);
}

vec4 uiPixel(int layer, ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(size.xy) - 1);
    switch (layer) {
    case 0: return texelFetch(nbg0, p, 0);
    case 1: return texelFetch(nbg1, p, 0);
    case 2: return texelFetch(nbg2, p, 0);
    default: return texelFetch(nbg3, p, 0);
    }
}

int uiClass(vec4 color) {
    if (color.a == 0.0) { return 0; }
    return max(max(color.r, color.g), color.b) < 0.125 ? 1 : 2;
}

// Reconstruct a sharp silhouette on an eight-sample grid, keeping the source
// colour and priority of the nearest pixel on the selected side of the edge.
vec4 uiContour(vec4 original, int layer, vec2 at) {
    if ((misc.x & (128 << layer)) == 0) { return original; }
    vec2 p = (floor(at * size.xy * 8.0) + 0.5) / 8.0 - 0.5;
    ivec2 q = ivec2(floor(p));
    vec2 f = fract(p);
    vec4 a = uiPixel(layer, q);
    vec4 b = uiPixel(layer, q + ivec2(1, 0));
    vec4 c = uiPixel(layer, q + ivec2(0, 1));
    vec4 d = uiPixel(layer, q + 1);
    ivec4 kinds = ivec4(uiClass(a), uiClass(b), uiClass(c), uiClass(d));
    if (all(equal(kinds, ivec4(kinds.x)))) { return original; }
    vec4 weights = vec4((1.0-f.x)*(1.0-f.y), f.x*(1.0-f.y), (1.0-f.x)*f.y, f.x*f.y);
    float opaque = dot(weights, mix(vec4(0), vec4(1), notEqual(kinds, ivec4(0))));
    if (opaque < 0.5) { return vec4(0); }
    float light = dot(weights, mix(vec4(0), vec4(1), equal(kinds, ivec4(2))));
    int kind = light >= 0.5 * opaque ? 2 : 1;
    if (kind == uiClass(original)) { return original; }
    weights = mix(vec4(-1), weights, equal(kinds, ivec4(kind)));
    if (weights.x >= max(max(weights.y, weights.z), weights.w)) { return a; }
    if (weights.y >= max(weights.z, weights.w)) { return b; }
    return weights.z >= weights.w ? c : d;
}

vec4 textLayer(vec4 original, int layer, vec2 at, ivec2 cell) {
    for (int i = 0; i < misc.w; ++i) {
        ivec4 area = hdText[i];
        int meta = hdTextMeta[i / 4][i % 4];
        if ((meta & 3) != layer || any(lessThan(cell, area.xy)) ||
            any(greaterThanEqual(cell, area.xy + area.zw))) { continue; }
        int scale = (meta & 4) != 0 ? 8 : 4;
        ivec2 p = clamp(ivec2(floor((at * size.xy - vec2(area.xy)) * float(scale))), ivec2(0), area.zw * scale - 1);
        uint address = uint((meta >> 3) + 4 * (p.x + p.y * area.z * scale));
        return vec4(hdByte(address), hdByte(address + 1u), hdByte(address + 2u), hdByte(address + 3u)) / 255.0;
    }
    return uiContour(original, layer, at);
}

vec3 pixel(vec2 at) {
    ivec2 cell = clamp(ivec2(floor(at * size.xy)), ivec2(0), ivec2(size.xy) - 1);
    bool outsideMenu = cell.x < misc.z || cell.x >= int(size.x) - misc.z;
    bool headerEdge = misc.z > 0 && menuBackground.x == 2 && cell.y < int(size.y) / 8 &&
                      (cell.x < misc.z + 64 || cell.x >= int(size.x) - misc.z - 64);
    if (outsideMenu || headerEdge) {
        vec4 edgeBack = texelFetch(back, ivec2(0, cell.y), 0);
        if (edgeBack.a < 0.5 || menuBackground.x == 0) {
            return vec3(0.0);
        }
        int x = misc.z + 4;
        if (menuBackground.x == 2) {
            // A full checker period continues with the same phase on either side.
            x = misc.z + ((cell.x - misc.z) % 32 + 32) % 32;
        }
        vec4 background = uiPixel(menuBackground.y, ivec2(x, cell.y));
        int layer = 2 + menuBackground.y;
        if (menuBackground.x == 2 && cell.y < int(size.y) / 8) {
            for (int i = 0; i < 4; ++i) {
                int headerX = cell.x < int(size.x) / 2
                    ? misc.z + min(cell.x, 64)
                    : int(size.x) - misc.z - 1 - min(int(size.x) - 1 - cell.x, 64);
                vec4 header = uiPixel(i, ivec2(headerX, cell.y));
                if (header.a > background.a) {
                    background = header;
                    layer = 2 + i;
                }
            }
        }
        return clamp(background.rgb + colorOffset[layer].rgb, 0.0, 1.0);
    }
    vec4 backColor = texelFetch(back, ivec2(0, cell.y), 0);
    if (backColor.a < 0.5) {
        return backColor.rgb;
    }

    // The sprite layer's pixel
    uint word = texelFetch(sprites, clamp(ivec2(floor(at * size.zw)), ivec2(0), ivec2(size.zw) - 1), 0).r;
    int type = sprite.x;
    bool mixed = sprite.y != 0;
    bool windowUsed = (sprite.z & 1) != 0;
    vec3 spriteColor = vec3(0.0);
    int priority = 0;
    int special = 0; // 0: nothing, 1: a pixel, 2: a shadow
    bool shadowBit = false;
    bool cut = texelFetch(window, cell, 0).r > 0.5;
    bool rgb = mixed && (word & 0x8000u) != 0u;
    bool rgbNothing = rgb && (type >= 8 ? (word & 0xFFu) == 0u : (type >= 2 && windowUsed && (word & 0x7FFFu) == 0u));
    if (cut || rgbNothing) {
    } else if (rgb) {
        spriteColor = vec3(word & 31u, (word >> 5) & 31u, (word >> 10) & 31u) / 31.0;
        // (5 bits to 8 as the hardware does: the 5 bits shifted up, not stretched)
        spriteColor = floor(spriteColor * 31.0 + 0.5) * 8.0 / 255.0;
        priority = priLow.x;
        special = 1;
    } else {
        ivec4 format = kFormats[type & 15];
        uint colorData = word & ((1u << uint(format.x)) - 1u);
        int priorityIndex = int((word >> uint(format.y)) & ((1u << uint(format.z)) - 1u));
        shadowBit = format.w != 0 && (word & 0x8000u) != 0u;
        special = (word & 0x7FFFu) == 0u ? 0 : (colorData == (1u << uint(format.x)) - 2u ? 2 : 1);
        if (windowUsed && (sprite.z & 2) != 0 && shadowBit != ((sprite.z & 4) != 0)) {
            shadowBit = true;
            special = 0;
        } else {
            spriteColor = texelFetch(colors, ivec2(int((uint(sprite.w) + colorData) & 2047u), 0), 0).rgb;
            priority = (special == 0 && !shadowBit) ? 0 : spritePriority(priorityIndex);
        }
    }

    // The layer on top
    int top = 6;
    int topPriority = 0;
    vec3 color = backColor.rgb;
    if ((sprite.z & 8) != 0 && special == 1) {
        if (priority > 0) {
            top = 0;
            color = spriteColor;
        }
        topPriority = priority;
    }
    vec4 layer;
    int layerPriority;
    layer = floorLayer(at);
    if (layer.a < 0.0) {
        layer = texelFetch(rbg0, cell, 0);
    }
    layerPriority = int(layer.a * 255.0 + 0.5);
    if (layerPriority > topPriority) { top = 1; topPriority = layerPriority; color = airportFloor(at, layer.rgb); }
    layer = textLayer(backgroundLayer(texelFetch(nbg0, cell, 0), 0, at), 0, at, cell);
    layerPriority = int(layer.a * 255.0 + 0.5);
    if (layerPriority > topPriority) { top = 2; topPriority = layerPriority; color = layer.rgb; }
    layer = textLayer(backgroundLayer(texelFetch(nbg1, cell, 0), 1, at), 1, at, cell);
    layerPriority = int(layer.a * 255.0 + 0.5);
    if (layerPriority > topPriority) { top = 3; topPriority = layerPriority; color = layer.rgb; }
    layer = textLayer(texelFetch(nbg2, cell, 0), 2, at, cell);
    layerPriority = int(layer.a * 255.0 + 0.5);
    if (layerPriority > topPriority) { top = 4; topPriority = layerPriority; color = layer.rgb; }
    layer = textLayer(texelFetch(nbg3, cell, 0), 3, at, cell);
    layerPriority = int(layer.a * 255.0 + 0.5);
    if (layerPriority > topPriority) { top = 5; topPriority = layerPriority; color = layer.rgb; }

    // A shadow pixel of the sprite layer halves the brightness of the layer shown, if that layer takes shadows
    bool shadowPixel = special == 2 || (!windowUsed && shadowBit);
    if (shadowPixel) {
        bool takes = top == 0 ? shadowBit : ((misc.x >> top) & 1) != 0;
        if (takes) {
            color = floor(color * 255.0 + 0.5) * 0.5 / 255.0;
            color = floor(color * 255.0) / 255.0;
        }
    }
    return clamp(color + colorOffset[top].rgb, 0.0, 1.0);
}

void main() {
    int n = max(misc.y, 1);
    vec2 dx = dFdx(uv), dy = dFdy(uv);
    vec3 sum = vec3(0.0);
    for (int j = 0; j < n; j++) {
        for (int i = 0; i < n; i++) {
            vec2 offset = (vec2(float(i), float(j)) + 0.5) / float(n) - 0.5;
            sum += pixel(uv + dx * offset.x + dy * offset.y);
        }
    }
    out_color = vec4(sum / float(n * n), 1.0);
}
