// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of satrecomp: https://github.com/tmcleroy/satrecomp
#include "gpu.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <zlib.h>

namespace satrecomp::video {

namespace {
#include "shaders.inc"

    struct Vertex {
        float position[2];
        float ab[4], cd[4];
        float clip[4], clipOut[4];
        uint32_t tex[4];
        uint32_t gouraud[4];
    };

    struct ComposeUniforms {
        float size[4];
        int32_t sprite[4];
        int32_t priLow[4], priHigh[4];
        int32_t misc[4];
        int32_t hdText[8][4];
        int32_t hdTextMeta[2][4];
        float colorOffset[7][4];
        int32_t floor[4];       // lines of a field, dots a line, flags, priority
        int32_t floorMore[4];   // colour offset, more character number bits, more palette bits, access
        uint32_t floorPlanes[16];
        uint32_t hdAirport[4];
        int32_t menuBackground[4];
    };

    constexpr uint32_t kVramWidth = 1024, kVramHeight = 512;
    constexpr uint32_t kVramBytes = kVramWidth * kVramHeight;
    constexpr uint32_t kHdScale = 4, kHdAtlasWidth = 16384, kHdMaxRows = 16384;
    constexpr uint32_t kWinnerWidth = 187, kWinnerHeight = 32, kWinnerTop = 360;
    constexpr uint32_t kWinnerLeftOfCenter = 93;
    // This pack format uses ordinary ZIP members without streaming data descriptors.
    // Read only the known members, directly from the archive.
    bool ReadTextureZip(const std::filesystem::path &path, std::string (&members)[4]) {
        std::ifstream file(path, std::ios::binary);
        constexpr size_t maxBytes = 256u * 1024 * 1024;
        constexpr const char *names[] = {"fighters.fmmtex", "winner.fmmhud", "fight-text.fmmhud", "airport-floor.fmmtex"};
        size_t total = 0;
        while (file) {
            uint8_t header[30];
            if (!file.read(reinterpret_cast<char *>(header), 4)) { return false; }
            if (std::memcmp(header, "PK\x01\x02", 4) == 0) { return !members[0].empty(); }
            if (std::memcmp(header, "PK\x03\x04", 4) != 0 ||
                !file.read(reinterpret_cast<char *>(header + 4), 26)) { return false; }
            auto word = [&](int i) { return static_cast<uint32_t>(header[i] | header[i + 1] << 8); };
            auto longword = [&](int i) { return word(i) | word(i + 2) << 16; };
            const uint32_t flags = word(6), method = word(8), crc = longword(14);
            const uint32_t compressed = longword(18), bytes = longword(22);
            if ((flags & ~0x800u) || compressed > maxBytes || bytes > maxBytes ||
                word(26) == 0 || word(26) > 256) { return false; }
            std::string name(word(26), '\0');
            if (!file.read(name.data(), name.size()) || !file.seekg(word(28), std::ios::cur)) { return false; }
            int index = 0;
            while (index < 4 && name != names[index]) { ++index; }
            if (index == 4) {
                file.seekg(compressed, std::ios::cur);
                continue;
            }
            if (!members[index].empty() || bytes == 0 || total + bytes > maxBytes) { return false; }
            auto &out = members[index];
            if (method == 0 && compressed == bytes) {
                out.resize(bytes);
                if (!file.read(out.data(), bytes)) { return false; }
            } else if (method == 8) {
                std::string input(compressed, '\0');
                if (!file.read(input.data(), compressed)) { return false; }
                out.resize(bytes);
                z_stream stream{};
                stream.next_in = reinterpret_cast<Bytef *>(input.data());
                stream.avail_in = compressed;
                stream.next_out = reinterpret_cast<Bytef *>(out.data());
                stream.avail_out = bytes;
                if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) { return false; }
                const int result = inflate(&stream, Z_FINISH);
                inflateEnd(&stream);
                if (result != Z_STREAM_END || stream.total_out != bytes || stream.total_in != compressed) { return false; }
            } else {
                return false;
            }
            if (crc32(0, reinterpret_cast<const Bytef *>(out.data()), bytes) != crc) { return false; }
            total += bytes;
        }
        return false;
    }
} // namespace

SDL_GPUShader *Gpu::Shader(SDL_GPUShaderStage stage, const unsigned char *spirv, size_t spirvSize, const char *msl,
                           uint32_t samplers, uint32_t uniforms) {
    SDL_GPUShaderCreateInfo info{};
    info.stage = stage;
    info.num_samplers = samplers;
    info.num_uniform_buffers = uniforms;
    if (SDL_GetGPUShaderFormats(m_device) & SDL_GPU_SHADERFORMAT_MSL) {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = reinterpret_cast<const Uint8 *>(msl);
        info.code_size = strlen(msl);
        info.entrypoint = "main0";
    } else {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv;
        info.code_size = spirvSize;
        info.entrypoint = "main";
    }
    SDL_GPUShader *shader = SDL_CreateGPUShader(m_device, &info);
    if (!shader) {
        fprintf(stderr, "video: shader: %s\n", SDL_GetError());
    }
    return shader;
}

SDL_GPUTexture *Gpu::Texture(SDL_GPUTextureFormat format, uint32_t width, uint32_t height, bool target) {
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | (target ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(m_device, &info);
    if (!texture) {
        fprintf(stderr, "video: texture %ux%u: %s\n", width, height, SDL_GetError());
    }
    return texture;
}

void Gpu::Release(SDL_GPUTexture *&texture) {
    if (texture) {
        SDL_ReleaseGPUTexture(m_device, texture);
        texture = nullptr;
    }
}

#define SATRECOMP_SHADER(stage, name, samplers, uniforms) \
    Shader(stage, k_##name##_spirv, sizeof(k_##name##_spirv), k_##name##_msl, samplers, uniforms)

bool Gpu::Init(SDL_GPUDevice *device) {
    m_device = device;
    SDL_GPUShader *quadVertex = SATRECOMP_SHADER(SDL_GPU_SHADERSTAGE_VERTEX, quad_vert, 0, 1);
    SDL_GPUShader *quadFragment = SATRECOMP_SHADER(SDL_GPU_SHADERSTAGE_FRAGMENT, quad_frag, 2, 1);
    SDL_GPUShader *screenVertex = SATRECOMP_SHADER(SDL_GPU_SHADERSTAGE_VERTEX, screen_vert, 0, 0);
    SDL_GPUShader *composeFragment = SATRECOMP_SHADER(SDL_GPU_SHADERSTAGE_FRAGMENT, compose_frag, 12, 1);
    SDL_GPUShader *hudFragment = SATRECOMP_SHADER(SDL_GPU_SHADERSTAGE_FRAGMENT, hud_frag, 1, 0);
    if (!quadVertex || !quadFragment || !screenVertex || !composeFragment) {
        return false;
    }
    if (hudFragment) {
        // The readout: a picture blended over the output by its alpha
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        target.blend_state.enable_blend = true;
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = screenVertex;
        info.fragment_shader = hudFragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        m_hudPipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
        SDL_ReleaseGPUShader(m_device, hudFragment);
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
        sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_linear = SDL_CreateGPUSampler(m_device, &sampler);
    }

    {
        SDL_GPUVertexBufferDescription buffer{};
        buffer.slot = 0;
        buffer.pitch = sizeof(Vertex);
        buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        const SDL_GPUVertexAttribute attributes[] = {
            {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, position)},
            {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, ab)},
            {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, cd)},
            {3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, clip)},
            {4, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, clipOut)},
            {5, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, offsetof(Vertex, tex)},
            {6, 0, SDL_GPU_VERTEXELEMENTFORMAT_UINT4, offsetof(Vertex, gouraud)},
        };
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R16_UINT;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = quadVertex;
        info.fragment_shader = quadFragment;
        info.vertex_input_state.vertex_buffer_descriptions = &buffer;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attributes;
        info.vertex_input_state.num_vertex_attributes = static_cast<Uint32>(SDL_arraysize(attributes));
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        m_quadPipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
    }
    {
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = screenVertex;
        info.fragment_shader = composeFragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        m_composePipeline = SDL_CreateGPUGraphicsPipeline(m_device, &info);
    }
    SDL_ReleaseGPUShader(m_device, quadVertex);
    SDL_ReleaseGPUShader(m_device, quadFragment);
    SDL_ReleaseGPUShader(m_device, screenVertex);
    SDL_ReleaseGPUShader(m_device, composeFragment);
    if (!m_quadPipeline || !m_composePipeline) {
        fprintf(stderr, "video: pipeline: %s\n", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_nearest = SDL_CreateGPUSampler(m_device, &sampler);

    m_vram = Texture(SDL_GPU_TEXTUREFORMAT_R8_UINT, kVramWidth, kVramHeight, false);
    m_colors = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 2048, 1, false);
    m_back = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, 1, 512, false);
    m_vram2 = Texture(SDL_GPU_TEXTUREFORMAT_R8_UINT, kVramWidth, kVramHeight, false);
    m_floorLines = Texture(SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT, 2, 512, false);
    return m_nearest && m_vram && m_colors && m_back && m_vram2 && m_floorLines;
}

void Gpu::Shutdown() {
    if (!m_device) {
        return;
    }
    SDL_WaitForGPUIdle(m_device);
    for (auto *&texture : m_sprites) {
        Release(texture);
    }
    for (auto *&texture : m_keptSprites) {
        Release(texture);
    }
    for (auto *&texture : m_layers) {
        Release(texture);
    }
    Release(m_vram);
    Release(m_hdAtlas);
    Release(m_colors);
    Release(m_back);
    Release(m_window);
    Release(m_vram2);
    Release(m_floorLines);
    Release(m_output);
    Release(m_hud);
    Release(m_plain);
    if (m_hudPipeline) {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_hudPipeline);
        m_hudPipeline = nullptr;
    }
    if (m_linear) {
        SDL_ReleaseGPUSampler(m_device, m_linear);
        m_linear = nullptr;
    }
    if (m_vertices) {
        SDL_ReleaseGPUBuffer(m_device, m_vertices);
    }
    if (m_transfer) {
        SDL_ReleaseGPUTransferBuffer(m_device, m_transfer);
    }
    if (m_download) {
        SDL_ReleaseGPUTransferBuffer(m_device, m_download);
    }
    if (m_nearest) {
        SDL_ReleaseGPUSampler(m_device, m_nearest);
    }
    if (m_quadPipeline) {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_quadPipeline);
    }
    if (m_composePipeline) {
        SDL_ReleaseGPUGraphicsPipeline(m_device, m_composePipeline);
    }
    m_device = nullptr;
}

void Gpu::SetOutputSize(uint32_t width, uint32_t height) {
    m_wantedWidth = std::max<uint32_t>(width, 16);
    m_wantedHeight = std::max<uint32_t>(height, 16);
}

void Gpu::CopySprites(SDL_GPUTexture *const from[2], SDL_GPUTexture *const to[2]) {
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
    for (int i = 0; i < 2; i++) {
        SDL_GPUTextureLocation source{}, destination{};
        source.texture = from[i];
        destination.texture = to[i];
        SDL_CopyGPUTextureToTexture(pass, &source, &destination, m_spriteWidth * m_scale, m_spriteHeight * m_scale, 1, false);
    }
    SDL_EndGPUCopyPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
}

void Gpu::KeepSprites() {
    if (!m_device || !m_sprites[0]) {
        return;
    }
    if (!m_keptSprites[0] || m_keptWidth != m_spriteWidth || m_keptHeight != m_spriteHeight || m_keptScale != m_scale) {
        for (auto *&texture : m_keptSprites) {
            Release(texture);
            texture = Texture(SDL_GPU_TEXTUREFORMAT_R16_UINT, m_spriteWidth * m_scale, m_spriteHeight * m_scale, true);
        }
        if (!m_keptSprites[0] || !m_keptSprites[1]) {
            return;
        }
    }
    m_keptWidth = m_spriteWidth;
    m_keptHeight = m_spriteHeight;
    m_keptScale = m_scale;
    CopySprites(m_sprites, m_keptSprites);
}

void Gpu::PutSpritesBack() {
    // (not when the frames run ahead changed the framebuffers' size: those are new, and what is drawn next
    // fills them)
    if (!m_device || !m_sprites[0] || !m_keptSprites[0] || m_keptWidth != m_spriteWidth || m_keptHeight != m_spriteHeight ||
        m_keptScale != m_scale) {
        return;
    }
    CopySprites(m_keptSprites, m_sprites);
}

// Textures and buffers of the sizes this frame needs
bool Gpu::Prepare(const Frame &frame) {
    // The VDP1's targets: enough resolution for the output (twice over, so edges can be smoothed), at most 8x
    uint32_t scale = m_fixedScale;
    if (scale == 0) {
        scale = (m_wantedHeight * 2 + frame.spriteHeight - 1) / frame.spriteHeight;
    }
    scale = std::clamp<uint32_t>(scale, 1, 8);
    if (!m_sprites[0] || frame.spriteWidth != m_spriteWidth || frame.spriteHeight != m_spriteHeight || scale != m_scale) {
        m_spriteWidth = frame.spriteWidth;
        m_spriteHeight = frame.spriteHeight;
        m_scale = scale;
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
        for (auto *&texture : m_sprites) {
            Release(texture);
            texture = Texture(SDL_GPU_TEXTUREFORMAT_R16_UINT, m_spriteWidth * scale, m_spriteHeight * scale, true);
            if (!texture) {
                return false;
            }
            SDL_GPUColorTargetInfo target{};
            target.texture = texture;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &target, 1, nullptr));
        }
        SDL_SubmitGPUCommandBuffer(cmd);
    }
    if (!m_layers[0] || frame.width != m_layerWidth || frame.height != m_layerHeight) {
        m_layerWidth = frame.width;
        m_layerHeight = frame.height;
        for (auto *&texture : m_layers) {
            Release(texture);
            texture = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, m_layerWidth, m_layerHeight, false);
        }
        Release(m_window);
        m_window = Texture(SDL_GPU_TEXTUREFORMAT_R8_UNORM, m_layerWidth, m_layerHeight, false);
        if (!m_layers[4] || !m_window) {
            return false;
        }
    }
    if (!m_output || m_outputWidth != m_wantedWidth || m_outputHeight != m_wantedHeight) {
        Release(m_output);
        m_outputWidth = m_wantedWidth;
        m_outputHeight = m_wantedHeight;
        m_output = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, m_outputWidth, m_outputHeight, true);
        if (!m_output) {
            return false;
        }
    }
    return true;
}

void Gpu::SetHdTexturePack(bool enabled) {
    if (enabled && !m_hdAttempted) {
        m_hdAttempted = true;
        if (!LoadHdTexturePack()) {
            SDL_Log("HD Texture Pack: no valid local ZIP selected");
        }
    }
    m_hdEnabled = enabled && !m_hdTextures.empty();
}

bool Gpu::LoadHdTexturePack() {
    if (m_hdPackPath.empty()) {
        return false;
    }
    const std::filesystem::path path(std::u8string(reinterpret_cast<const char8_t *>(m_hdPackPath.c_str())));
    std::string members[4];
    if (!ReadTextureZip(path, members)) {
        return false;
    }
    std::istringstream file(std::move(members[0]), std::ios::binary);
    char magic[8];
    if (!file.read(magic, sizeof(magic)) || std::memcmp(magic, "FMMHD001", sizeof(magic))) {
        return false;
    }
    // Each record is little-endian width and height, then original and 4x RGB555 images in VDP1 byte order.
    std::vector<HdTexture> textures;
    std::vector<uint8_t> pixels;
    while (file.peek() != std::char_traits<char>::eof()) {
        uint8_t size[4];
        if (!file.read(reinterpret_cast<char *>(size), sizeof(size))) {
            return false;
        }
        const auto width = static_cast<uint16_t>(size[0] | size[1] << 8);
        const auto height = static_cast<uint16_t>(size[2] | size[3] << 8);
        const uint32_t nativeBytes = 2u * width * height;
        const size_t offset = pixels.size();
        if (width == 0 || width > 504 || height == 0 || height > 255 ||
            offset + nativeBytes * kHdScale * kHdScale > kHdAtlasWidth * kHdMaxRows) {
            return false;
        }
        HdTexture texture{width, height, static_cast<uint32_t>(offset), std::vector<uint8_t>(nativeBytes)};
        pixels.resize(offset + nativeBytes * kHdScale * kHdScale);
        if (!file.read(reinterpret_cast<char *>(texture.original.data()), nativeBytes) ||
            !file.read(reinterpret_cast<char *>(pixels.data() + offset), nativeBytes * kHdScale * kHdScale)) {
            return false;
        }
        textures.push_back(std::move(texture));
    }
    std::vector<HdText> text;
    const auto readText = [&](std::istream &input, HdText entry) {
        const size_t count = static_cast<size_t>(entry.width) * entry.height;
        const size_t hdBytes = count * 4 * entry.scale * entry.scale;
        if (!count || (entry.layer & ~0x80u) >= 4 || (entry.scale != 4 && entry.scale != 8) || pixels.size() + hdBytes > kHdAtlasWidth * kHdMaxRows) {
            return false;
        }
        entry.offset = static_cast<uint32_t>(pixels.size());
        entry.original.resize(count);
        pixels.resize(entry.offset + hdBytes);
        if (!input.read(reinterpret_cast<char *>(entry.original.data()), count * 4) ||
            !input.read(reinterpret_cast<char *>(pixels.data() + entry.offset), hdBytes)) {
            pixels.resize(entry.offset);
            return false;
        }
        text.push_back(std::move(entry));
        return true;
    };
    std::istringstream winner(std::move(members[1]), std::ios::binary);
    if (winner.read(magic, sizeof(magic)) && std::memcmp(magic, "FMMWIN01", sizeof(magic)) == 0) {
        readText(winner, {-static_cast<int32_t>(kWinnerLeftOfCenter), kWinnerTop, kWinnerWidth, kWinnerHeight, 2, 4, 0, {}});
    }
    std::istringstream messages(std::move(members[2]), std::ios::binary);
    if (messages.read(magic, sizeof(magic)) &&
        (std::memcmp(magic, "FMMTXT01", sizeof(magic)) == 0 || std::memcmp(magic, "FMMTXT02", sizeof(magic)) == 0)) {
        const bool hasScale = magic[7] == '2';
        uint8_t header[10];
        while (messages.read(reinterpret_cast<char *>(header), hasScale ? 10 : 9)) {
            const auto word = [&](int i) { return static_cast<uint32_t>(header[i] | header[i + 1] << 8); };
            if (!readText(messages, {static_cast<int16_t>(word(0)), word(2), word(4), word(6), header[8], hasScale ? static_cast<uint32_t>(header[9]) : 4u, 0, {}})) {
                break;
            }
        }
    }
    const auto &airport = members[3];
    constexpr size_t airportBytes = (2048u * 2048 + 1024u * 1024) * 3;
    if (airport.size() == 20 + airportBytes && airport.compare(0, 8, "FMMAIR02") == 0 &&
        pixels.size() + airportBytes <= kHdAtlasWidth * kHdMaxRows) {
        for (uint32_t i = 0; i < 12; ++i) {
            m_hdAirportCrc[i / 4] |= static_cast<uint32_t>(static_cast<uint8_t>(airport[8 + i])) << (8 * (i % 4));
        }
        m_hdAirportOffset = static_cast<uint32_t>(pixels.size());
        pixels.insert(pixels.end(), airport.begin() + 20, airport.end());
    }
    pixels.resize((pixels.size() + kHdAtlasWidth - 1) / kHdAtlasWidth * kHdAtlasWidth);
    if (pixels.empty()) {
        return false;
    }
    const uint32_t rows = static_cast<uint32_t>(pixels.size() / kHdAtlasWidth);
    m_hdAtlas = Texture(SDL_GPU_TEXTUREFORMAT_R8_UINT, kHdAtlasWidth, rows, false);
    if (!m_hdAtlas) {
        return false;
    }
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = static_cast<uint32_t>(pixels.size());
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(m_device, &info);
    if (!transfer) {
        Release(m_hdAtlas);
        return false;
    }
    bool uploaded = false;
    if (void *mapped = SDL_MapGPUTransferBuffer(m_device, transfer, false)) {
        std::memcpy(mapped, pixels.data(), pixels.size());
        SDL_UnmapGPUTransferBuffer(m_device, transfer);
        if (SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device)) {
            SDL_GPUTextureTransferInfo source{transfer, 0, kHdAtlasWidth, rows};
            SDL_GPUTextureRegion region{};
            region.texture = m_hdAtlas;
            region.w = kHdAtlasWidth;
            region.h = rows;
            region.d = 1;
            SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
            SDL_UploadToGPUTexture(pass, &source, &region, false);
            SDL_EndGPUCopyPass(pass);
            uploaded = SDL_SubmitGPUCommandBuffer(cmd);
        }
    }
    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
    if (!uploaded) {
        Release(m_hdAtlas);
        return false;
    }
    m_hdText = std::move(text);
    const auto sizeKey = [](const HdTexture &texture) {
        return (static_cast<uint32_t>(texture.width) << 16) | texture.height;
    };
    std::sort(textures.begin(), textures.end(), [&](const HdTexture &a, const HdTexture &b) {
        return sizeKey(a) < sizeKey(b);
    });
    m_hdTextures = std::move(textures);
    return true;
}

bool Gpu::MatchesHdText(const Frame &frame, const HdText &text) const {
    const int32_t left = static_cast<int32_t>(frame.width / 2) + text.leftFromCenter;
    if (left < 0 || left + text.width > frame.width || text.top + text.height > frame.height) {
        return false;
    }
    for (uint32_t y = 0; y < text.height; ++y) {
        const uint32_t *row = frame.layers[(text.layer & 3) + 1] + (text.top + y) * frame.width + left;
        for (uint32_t x = 0; x < text.width; ++x) {
            const uint32_t value = row[x] >> 24 ? row[x] : 0;
            const uint32_t original = text.original[y * text.width + x];
            if (value == original) {
                continue;
            }
            const auto warm = [](uint32_t rgba) {
                const int red = rgba & 255, blue = (rgba >> 16) & 255;
                return red > 100 && red - blue > 45;
            };
            if (!(text.layer & 0x80) || (value >> 24) != (original >> 24) ||
                warm(value) != warm(original)) {
                return false;
            }
        }
    }
    return true;
}

void Gpu::Draw(const Frame &frame) {
    if (!m_device || !frame.width || !frame.height || !Prepare(frame)) {
        return;
    }
    m_hudApplied = false;
    if (frame.resetSprites) {
        SDL_GPUCommandBuffer *clear = SDL_AcquireGPUCommandBuffer(m_device);
        for (SDL_GPUTexture *texture : m_sprites) {
            SDL_GPUColorTargetInfo target{};
            target.texture = texture;
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_EndGPURenderPass(SDL_BeginGPURenderPass(clear, &target, 1, nullptr));
        }
        SDL_SubmitGPUCommandBuffer(clear);
    }
    const uint32_t layerBytes = frame.width * frame.height * 4;
    const uint32_t vertexBytes = static_cast<uint32_t>(frame.quads.size() * 6 * sizeof(Vertex));
    const uint32_t vramBytes = kVramBytes;
    const uint32_t backRows = std::min<uint32_t>(frame.height, 512);

    // Everything to upload goes through one transfer buffer: [vertices][VRAM snapshots][layers][window][back][colours]
    uint32_t offset = 0;
    auto reserve = [&](uint32_t size) {
        const uint32_t at = offset;
        offset += (size + 15) & ~15u;
        return at;
    };
    const uint32_t vertexAt = reserve(vertexBytes);
    std::vector<uint32_t> vramAt;
    for (size_t i = 0; i < frame.vram.size(); i++) {
        vramAt.push_back(reserve(vramBytes));
    }
    uint32_t layerAt[5];
    for (uint32_t &at : layerAt) {
        at = reserve(layerBytes);
    }
    const uint32_t windowAt = reserve(frame.width * frame.height);
    const uint32_t backAt = reserve(512 * 4);
    const uint32_t colorsAt = reserve(2048 * 4);
    const uint32_t vram2At = reserve(vramBytes);
    const uint32_t floorAt = reserve(512 * 8 * sizeof(float));
    if (offset > m_transferCapacity) {
        if (m_transfer) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_transfer);
        }
        m_transferCapacity = offset + offset / 2;
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = m_transferCapacity;
        m_transfer = SDL_CreateGPUTransferBuffer(m_device, &info);
    }
    if (vertexBytes > m_vertexCapacity) {
        if (m_vertices) {
            SDL_ReleaseGPUBuffer(m_device, m_vertices);
        }
        m_vertexCapacity = std::max<uint32_t>(vertexBytes * 2, 1024 * 6 * sizeof(Vertex));
        SDL_GPUBufferCreateInfo info{};
        info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        info.size = m_vertexCapacity;
        m_vertices = SDL_CreateGPUBuffer(m_device, &info);
    }
    auto *bytes = static_cast<uint8_t *>(SDL_MapGPUTransferBuffer(m_device, m_transfer, true));
    if (!bytes) {
        return;
    }

    // Each quad as the rectangle around it (two triangles), no further than its clipping rectangle
    auto *vertex = reinterpret_cast<Vertex *>(bytes + vertexAt);
    size_t eventIndex = 0;
    for (const Quad &quad : frame.quads) {
        float x0 = quad.corner[0][0], x1 = x0, y0 = quad.corner[0][1], y1 = y0;
        for (int i = 1; i < 4; i++) {
            x0 = std::min(x0, quad.corner[i][0]);
            x1 = std::max(x1, quad.corner[i][0]);
            y0 = std::min(y0, quad.corner[i][1]);
            y1 = std::max(y1, quad.corner[i][1]);
        }
        x0 = std::max(x0, quad.clip[0]);
        y0 = std::max(y0, quad.clip[1]);
        x1 = std::max(x0, std::min(x1, quad.clip[2]));
        y1 = std::max(y0, std::min(y1, quad.clip[3]));
        Vertex v{};
        memcpy(v.ab, quad.corner[0], sizeof(float) * 4);
        memcpy(v.cd, quad.corner[2], sizeof(float) * 4);
        memcpy(v.clip, quad.clip, sizeof(v.clip));
        memcpy(v.clipOut, quad.clipOut, sizeof(v.clipOut));
        v.tex[0] = quad.texture;
        v.tex[1] = quad.width | (static_cast<uint32_t>(quad.height) << 10) | (static_cast<uint32_t>(quad.lines) << 18);
        v.tex[2] = quad.mode | (static_cast<uint32_t>(quad.flags) << 16);
        v.tex[3] = quad.color;
        if (m_hdEnabled && (quad.flags & kQuadTextureUpscale) && ((quad.mode >> 3) & 7) == 5) {
            const auto quadIndex = static_cast<size_t>(&quad - frame.quads.data());
            while (eventIndex < frame.events.size() &&
                   (frame.events[eventIndex].kind != Vdp1Event::Kind::Draw ||
                    quadIndex >= frame.events[eventIndex].firstQuad + frame.events[eventIndex].quadCount)) {
                ++eventIndex;
            }
            if (eventIndex < frame.events.size() && quadIndex >= frame.events[eventIndex].firstQuad) {
                const auto &vram = frame.vram[frame.events[eventIndex].vram];
                const uint32_t key = (static_cast<uint32_t>(quad.width) << 16) | quad.height;
                auto match = std::lower_bound(m_hdTextures.begin(), m_hdTextures.end(), key,
                    [](const HdTexture &texture, uint32_t value) {
                        return ((static_cast<uint32_t>(texture.width) << 16) | texture.height) < value;
                    });
                for (; match != m_hdTextures.end() && match->width == quad.width && match->height == quad.height; ++match) {
                    const auto &texture = *match;
                    if (quad.texture + texture.original.size() <= vram.size() &&
                        std::memcmp(vram.data() + quad.texture, texture.original.data(), texture.original.size()) == 0) {
                        v.tex[0] = texture.offset;
                        v.tex[2] |= static_cast<uint32_t>(kQuadHdTexture) << 16;
                        break;
                    }
                }
            }
        }
        for (int i = 0; i < 4; i++) {
            v.gouraud[i] = quad.gouraud[i];
        }
        const float rect[6][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}};
        for (const auto &corner : rect) {
            v.position[0] = corner[0];
            v.position[1] = corner[1];
            *vertex++ = v;
        }
    }
    for (size_t i = 0; i < frame.vram.size(); i++) {
        memcpy(bytes + vramAt[i], frame.vram[i].data(), std::min<size_t>(vramBytes, frame.vram[i].size()));
    }
    for (int i = 0; i < 5; i++) {
        memcpy(bytes + layerAt[i], frame.layers[i], layerBytes);
    }
    for (uint32_t i = 0; i < frame.width * frame.height; i++) {
        bytes[windowAt + i] = frame.spriteWindow[i] ? 255 : 0;
    }
    memcpy(bytes + backAt, frame.back, backRows * 4);
    memcpy(bytes + colorsAt, frame.colors, 2048 * 4);
    memcpy(bytes + vram2At, frame.floor.vram, vramBytes);
    memcpy(bytes + floorAt, frame.floor.lines, 512 * 8 * sizeof(float));
    SDL_UnmapGPUTransferBuffer(m_device, m_transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    auto uploadTexture = [&](SDL_GPUCopyPass *pass, SDL_GPUTexture *texture, uint32_t at, uint32_t width, uint32_t height) {
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = m_transfer;
        source.offset = at;
        source.pixels_per_row = width;
        source.rows_per_layer = height;
        SDL_GPUTextureRegion region{};
        region.texture = texture;
        region.w = width;
        region.h = height;
        region.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &region, true);
    };
    {
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
        if (vertexBytes) {
            SDL_GPUTransferBufferLocation source{m_transfer, vertexAt};
            SDL_GPUBufferRegion region{m_vertices, 0, vertexBytes};
            SDL_UploadToGPUBuffer(pass, &source, &region, true);
        }
        for (int i = 0; i < 5; i++) {
            uploadTexture(pass, m_layers[i], layerAt[i], frame.width, frame.height);
        }
        uploadTexture(pass, m_window, windowAt, frame.width, frame.height);
        uploadTexture(pass, m_back, backAt, 1, 512);
        uploadTexture(pass, m_colors, colorsAt, 2048, 1);
        uploadTexture(pass, m_vram2, vram2At, kVramWidth, kVramHeight);
        uploadTexture(pass, m_floorLines, floorAt, 2, 512);
        SDL_EndGPUCopyPass(pass);
    }

    // The VDP1's work, in the order it happened
    const float vertexUniforms[4] = {static_cast<float>(m_spriteWidth), static_cast<float>(m_spriteHeight), 0, 0};
    const float fragmentUniforms[4] = {static_cast<float>(m_scale), m_textureUpscale ? 1.0f : 0.0f, 0, 0};
    for (const Vdp1Event &event : frame.events) {
        if (event.kind == Vdp1Event::Kind::Swap) {
            std::swap(m_sprites[0], m_sprites[1]);
            continue;
        }
        const bool erase = event.kind == Vdp1Event::Kind::Erase;
        if (!erase) {
            SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
            uploadTexture(pass, m_vram, vramAt[event.vram], kVramWidth, kVramHeight);
            SDL_EndGPUCopyPass(pass);
        }
        if (!event.quadCount) {
            continue;
        }
        SDL_GPUColorTargetInfo target{};
        target.texture = m_sprites[erase ? 0 : 1];
        target.load_op = SDL_GPU_LOADOP_LOAD;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, m_quadPipeline);
        const SDL_GPUBufferBinding binding{m_vertices, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        const SDL_GPUTextureSamplerBinding samplers[] = {
            {m_vram, m_nearest}, {m_hdEnabled ? m_hdAtlas : m_vram, m_nearest},
        };
        SDL_BindGPUFragmentSamplers(pass, 0, samplers, 2);
        SDL_PushGPUVertexUniformData(cmd, 0, vertexUniforms, sizeof(vertexUniforms));
        SDL_PushGPUFragmentUniformData(cmd, 0, fragmentUniforms, sizeof(fragmentUniforms));
        SDL_DrawGPUPrimitives(pass, event.quadCount * 6, 1, event.firstQuad * 6, 0);
        SDL_EndGPURenderPass(pass);
    }

    // The picture
    {
        ComposeUniforms uniforms{};
        uniforms.size[0] = static_cast<float>(frame.width);
        uniforms.size[1] = static_cast<float>(frame.height);
        uniforms.size[2] = static_cast<float>(m_spriteWidth * m_scale);
        uniforms.size[3] = static_cast<float>(m_spriteHeight * m_scale);
        uniforms.sprite[0] = static_cast<int32_t>(frame.spriteType);
        uniforms.sprite[1] = frame.spriteMixed;
        uniforms.sprite[2] = (frame.spriteWindowUsed ? 1 : 0) | (frame.spriteWindowEnabled ? 2 : 0) |
                             (frame.spriteWindowInverted ? 4 : 0) | (frame.spritesEnabled ? 8 : 0);
        uniforms.sprite[3] = static_cast<int32_t>(frame.spriteColorOffset);
        for (int i = 0; i < 4; i++) {
            uniforms.priLow[i] = frame.spritePriorities[i];
            uniforms.priHigh[i] = frame.spritePriorities[i + 4];
        }
        for (int i = 0; i < 7; i++) {
            uniforms.misc[0] |= frame.shadow[i] ? 1 << i : 0;
        }
        for (int i = 0; i < 7; i++) {
            for (int c = 0; c < 3; c++) {
                uniforms.colorOffset[i][c] = frame.colorOffset[i][c] / 255.0f;
            }
        }
        const Frame::Floor &floor = frame.floor;
        uniforms.floor[0] = static_cast<int32_t>(floor.lineCount);
        uniforms.floor[1] = static_cast<int32_t>(floor.dots);
        uniforms.floor[2] = (floor.fourCell ? 1 : 0) | (floor.twoWord ? 2 : 0) | (floor.extended ? 4 : 0) | (floor.colors256 ? 8 : 0) |
                            (floor.transparency ? 16 : 0) | (floor.priorityPerCharacter ? 32 : 0) | (floor.specialPriority ? 64 : 0) |
                            (floor.over << 8) | (floor.pageShiftH << 12) | (floor.pageShiftV << 13);
        uniforms.floor[3] = floor.priority;
        uniforms.floorMore[0] = static_cast<int32_t>(floor.colorOffset);
        uniforms.floorMore[1] = static_cast<int32_t>(floor.moreNumber);
        uniforms.floorMore[2] = static_cast<int32_t>(floor.morePalette);
        uniforms.floorMore[3] = floor.access;
        memcpy(uniforms.floorPlanes, floor.planes, sizeof(uniforms.floorPlanes));
        if (m_hdEnabled && m_hdAirportOffset && floor.planes[5] == 0x20000 && floor.planes[6] == 0x22000 &&
            floor.planes[9] == 0x24000 && floor.planes[10] == 0x26000 &&
            crc32(0, reinterpret_cast<const Bytef *>(floor.vram) + 0x20000, 0xa000) == m_hdAirportCrc[0] &&
            crc32(0, reinterpret_cast<const Bytef *>(floor.vram) + 0x60000, 124608) == m_hdAirportCrc[1] &&
            crc32(0, reinterpret_cast<const Bytef *>(frame.colors + 768), 256 * 4) == m_hdAirportCrc[2]) {
            uniforms.hdAirport[0] = m_hdAirportOffset;
        }
        // (several samples a pixel when the VDP1's target has more pixels than the output shows)
        const uint32_t samples = std::clamp<uint32_t>((m_spriteHeight * m_scale + m_outputHeight / 2) / m_outputHeight, 1, 4);
        uniforms.misc[1] = static_cast<int32_t>(samples);
        if (m_uiContours) {
            // The normal backgrounds carry the interface. In 3D scenes, NBG0
            // and NBG1 can carry scenery, so only process the HUD layers there.
            bool arena = false;
            for (uint32_t row = 0; row < floor.lineCount && !arena; ++row) {
                arena = floor.lines[row * 8 + 4] != 0.0f;
            }
            uniforms.misc[0] |= (arena ? 0xC : 0xF) << 7;
        }
        uniforms.misc[2] = static_cast<int32_t>(frame.blankSides);
        if (frame.blankSides && frame.layers[1] && frame.width > 2 * frame.blankSides + 64 && frame.height > 8) {
            const uint32_t left = frame.blankSides;
            for (uint32_t menuLayer = 0; menuLayer < 4; ++menuLayer) {
                const auto background = [&](uint32_t x, uint32_t y) {
                    return frame.layers[menuLayer + 1][static_cast<size_t>(y) * frame.width + x];
                };
                const uint32_t row = frame.height / 2;
                const uint32_t a = background(left + 4, row), b = background(left + 20, row);
                const auto blue = [](uint32_t color) {
                    return (color & 0xFFFFu) == 0 && (color & 0xFF0000u) != 0 && (color >> 24) != 0;
                };
                const auto gray = [](uint32_t color) {
                    const uint32_t r = color & 255u;
                    return r > 0 && r == ((color >> 8) & 255u) && r == ((color >> 16) & 255u) && (color >> 24) != 0;
                };
                if (blue(a) && blue(b) && a != b &&
                    a == background(left + 36, row) && b == background(left + 52, row)) {
                    uniforms.menuBackground[0] = 2;
                    uniforms.menuBackground[1] = menuLayer;
                    break;
                } else if (menuLayer == 0 && gray(a) && a == b && gray(background(left + 4, 0)) &&
                           gray(background(left + 4, frame.height - 1))) {
                    uniforms.menuBackground[0] = 1;
                    break;
                }
            }
        }

        if (m_hdEnabled) {
            for (const auto &text : m_hdText) {
                if (!MatchesHdText(frame, text)) {
                    continue;
                }
                const int i = uniforms.misc[3]++;
                uniforms.hdTextMeta[i / 4][i % 4] = static_cast<int32_t>((text.offset << 3) |
                    (text.scale == 8 ? 4u : 0u) | (text.layer & 3));
                uniforms.hdText[i][0] = static_cast<int32_t>(frame.width / 2) + text.leftFromCenter;
                uniforms.hdText[i][1] = static_cast<int32_t>(text.top);
                uniforms.hdText[i][2] = static_cast<int32_t>(text.width);
                uniforms.hdText[i][3] = static_cast<int32_t>(text.height);
                if (uniforms.misc[3] == 8) { break; }
            }
        }

        SDL_GPUColorTargetInfo target{};
        target.texture = m_output;
        target.load_op = SDL_GPU_LOADOP_DONT_CARE;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, m_composePipeline);
        const SDL_GPUTextureSamplerBinding samplers[12] = {
            {m_sprites[0], m_nearest}, {m_colors, m_nearest},    {m_layers[0], m_nearest},  {m_layers[1], m_nearest},
            {m_layers[2], m_nearest},  {m_layers[3], m_nearest}, {m_layers[4], m_nearest},  {m_window, m_nearest},
            {m_back, m_nearest},       {m_vram2, m_nearest},     {m_floorLines, m_nearest},
            {m_hdEnabled ? m_hdAtlas : m_vram, m_nearest},
        };
        SDL_BindGPUFragmentSamplers(pass, 0, samplers, 12);
        SDL_PushGPUFragmentUniformData(cmd, 0, &uniforms, sizeof(uniforms));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
}

bool Gpu::Download(SDL_GPUTexture *texture, uint32_t width, uint32_t height, uint32_t bytesPerPixel, void *out) {
    const uint32_t size = width * height * bytesPerPixel;
    if (size > m_downloadCapacity) {
        if (m_download) {
            SDL_ReleaseGPUTransferBuffer(m_device, m_download);
        }
        m_downloadCapacity = size;
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        info.size = size;
        m_download = SDL_CreateGPUTransferBuffer(m_device, &info);
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.w = width;
    region.h = height;
    region.d = 1;
    SDL_GPUTextureTransferInfo destination{};
    destination.transfer_buffer = m_download;
    destination.pixels_per_row = width;
    destination.rows_per_layer = height;
    SDL_DownloadFromGPUTexture(pass, &region, &destination);
    SDL_EndGPUCopyPass(pass);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence) {
        return false;
    }
    SDL_WaitForGPUFences(m_device, true, &fence, 1);
    SDL_ReleaseGPUFence(m_device, fence);
    const void *data = SDL_MapGPUTransferBuffer(m_device, m_download, false);
    if (!data) {
        return false;
    }
    memcpy(out, data, size);
    SDL_UnmapGPUTransferBuffer(m_device, m_download);
    return true;
}

bool Gpu::Read(std::vector<uint32_t> &pixels) {
    if (!m_output) {
        return false;
    }
    pixels.resize(static_cast<size_t>(m_outputWidth) * m_outputHeight);
    return Download(m_output, m_outputWidth, m_outputHeight, 4, pixels.data());
}

bool Gpu::ReadSprites(std::vector<uint16_t> &words, uint32_t &width, uint32_t &height) {
    if (!m_sprites[0]) {
        return false;
    }
    width = m_spriteWidth * m_scale;
    height = m_spriteHeight * m_scale;
    words.resize(static_cast<size_t>(width) * height);
    return Download(m_sprites[0], width, height, 2, words.data());
}

void Gpu::SetOverlay(const uint32_t *pixels, uint32_t width, uint32_t height) {
    if (!m_device || !width || !height) {
        return;
    }
    if (!m_overlay || width != m_overlayWidth || height != m_overlayHeight) {
        Release(m_overlay);
        m_overlay = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, false);
        m_overlayWidth = width;
        m_overlayHeight = height;
        if (!m_overlay) {
            return;
        }
    }
    const uint32_t size = width * height * 4;
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(m_device, &info);
    if (!transfer) {
        return;
    }
    if (void *bytes = SDL_MapGPUTransferBuffer(m_device, transfer, false)) {
        memcpy(bytes, pixels, size);
        SDL_UnmapGPUTransferBuffer(m_device, transfer);
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transfer;
        source.pixels_per_row = width;
        source.rows_per_layer = height;
        SDL_GPUTextureRegion region{};
        region.texture = m_overlay;
        region.w = width;
        region.h = height;
        region.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &region, false);
        SDL_EndGPUCopyPass(pass);
        SDL_SubmitGPUCommandBuffer(cmd);
        m_overlayShown = true;
    }
    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
}

void Gpu::SetHudLayer(const uint32_t *pixels, uint32_t width, uint32_t height) {
    if (!m_device || !m_hudPipeline || !width || !height) {
        return;
    }
    if (!m_hud || width != m_hudWidth || height != m_hudHeight) {
        Release(m_hud);
        m_hud = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, width, height, false);
        m_hudWidth = width;
        m_hudHeight = height;
        if (!m_hud) {
            m_hudShown = false;
            return;
        }
    }
    const uint32_t size = width * height * 4;
    SDL_GPUTransferBufferCreateInfo info{};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(m_device, &info);
    if (!transfer) {
        return;
    }
    if (void *bytes = SDL_MapGPUTransferBuffer(m_device, transfer, false)) {
        memcpy(bytes, pixels, size);
        SDL_UnmapGPUTransferBuffer(m_device, transfer);
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
        SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = transfer;
        source.pixels_per_row = width;
        source.rows_per_layer = height;
        SDL_GPUTextureRegion region{};
        region.texture = m_hud;
        region.w = width;
        region.h = height;
        region.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &region, false);
        SDL_EndGPUCopyPass(pass);
        SDL_SubmitGPUCommandBuffer(cmd);
        m_hudShown = true;
        m_hudChanged = true;
    }
    SDL_ReleaseGPUTransferBuffer(m_device, transfer);
}

// Blends the readout into the output: once for each picture drawn, and again when the readout changes
// while the picture stays (the game stopped, a note)
void Gpu::ApplyHud() {
    if (!m_output || !m_hudPipeline || (m_hudApplied && !m_hudChanged) || (!m_hudApplied && !m_hudShown)) {
        m_hudChanged = false;
        return;
    }
    const bool again = m_hudApplied; // (the readout that is on the picture is not the one to show any more)
    m_hudApplied = true;
    m_hudChanged = false;
    if (!m_plain || m_plainWidth != m_outputWidth || m_plainHeight != m_outputHeight) {
        if (again) {
            return; // (no picture without the readout was kept: the next one drawn will be right)
        }
        Release(m_plain);
        m_plain = Texture(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, m_outputWidth, m_outputHeight, true);
        m_plainWidth = m_outputWidth;
        m_plainHeight = m_outputHeight;
        if (!m_plain) {
            return;
        }
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    {
        SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureLocation from{}, to{};
        from.texture = again ? m_plain : m_output;
        to.texture = again ? m_output : m_plain;
        SDL_CopyGPUTextureToTexture(copy, &from, &to, m_outputWidth, m_outputHeight, 1, false);
        SDL_EndGPUCopyPass(copy);
    }
    if (m_hudShown && m_hud) {
        SDL_GPUColorTargetInfo target{};
        target.texture = m_output;
        target.load_op = SDL_GPU_LOADOP_LOAD;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, m_hudPipeline);
        const SDL_GPUTextureSamplerBinding sampler{m_hud, m_linear};
        SDL_BindGPUFragmentSamplers(pass, 0, &sampler, 1);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
}

bool Gpu::ReadShown(std::vector<uint32_t> &pixels) {
    ApplyHud();
    return Read(pixels);
}

SDL_FRect FitPicture(int width, int height, float columns, float lines, float aspect, int scaling, int shape) {
    // The picture's shape: its own; with square pixels, as many times wider than tall as it has columns for
    // lines (352 x 224 is 11:7; interlaced 704 x 448 the same); or the window's
    const float window = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : aspect;
    const float ratio = shape == 1 ? columns / lines : shape == 2 ? window : aspect;
    SDL_FRect where{};
    if (scaling == 1) {
        // (as many of the window's rows to one of the picture's as fits with the picture's width too)
        const int rows = static_cast<int>(lines > 300.0f ? lines / 2.0f : lines);
        for (int by = height / std::max(rows, 1); by >= 1; by--) {
            where.h = static_cast<float>(by * rows);
            where.w = shape == 2 ? static_cast<float>(width) : std::round(where.h * ratio);
            if (where.w <= static_cast<float>(width)) {
                where.x = std::floor((static_cast<float>(width) - where.w) / 2.0f);
                where.y = std::floor((static_cast<float>(height) - where.h) / 2.0f);
                return where;
            }
        }
    }
    where.w = std::min(static_cast<float>(width), std::round(static_cast<float>(height) * ratio));
    where.h = shape == 2 ? static_cast<float>(height) : std::round(where.w / ratio);
    where.x = std::floor((static_cast<float>(width) - where.w) / 2.0f);
    where.y = std::floor((static_cast<float>(height) - where.h) / 2.0f);
    return where;
}

SDL_FRect Gpu::Fit(int width, int height) const {
    return FitPicture(width, height, m_layerWidth ? static_cast<float>(m_layerWidth) : 352.0f,
                      m_layerHeight ? static_cast<float>(m_layerHeight) : 224.0f, m_aspect, m_scaling, m_shape);
}

void Gpu::SizeFor(int width, int height) {
    if (m_native || (m_scaling == 2 && m_fixedScale)) {
        // (the Saturn's own size, times the fixed resolution's when smoothed up: a shader takes it as it is)
        const uint32_t times = m_native ? 1 : m_fixedScale;
        const uint32_t lines = m_layerHeight ? m_layerHeight : 224;
        SetOutputSize(static_cast<uint32_t>(static_cast<float>(lines) * m_aspect + 0.5f) * times, lines * times);
        return;
    }
    const SDL_FRect where = Fit(width, height);
    SetOutputSize(static_cast<uint32_t>(where.w), static_cast<uint32_t>(where.h));
}

void Gpu::Present(SDL_Window *window) {
    if (!(m_overlayShown && m_overlay)) {
        ApplyHud();
    }
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(m_device);
    SDL_GPUTexture *swapchain = nullptr;
    Uint32 width = 0, height = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchain, &width, &height) || !swapchain) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return;
    }
    // As SetFit says (as large as fits at the picture's shape, by default), black around it
    const SDL_FRect where = Fit(static_cast<int>(width), static_cast<int>(height));
    SizeFor(static_cast<int>(width), static_cast<int>(height));
    {
        SDL_GPUColorTargetInfo target{};
        target.texture = swapchain;
        target.clear_color = {0, 0, 0, 1};
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_EndGPURenderPass(SDL_BeginGPURenderPass(cmd, &target, 1, nullptr));
    }
    if (m_output) {
        const bool overlay = m_overlayShown && m_overlay;
        SDL_GPUBlitInfo blit{};
        blit.source.texture = overlay ? m_overlay : m_output;
        blit.source.w = overlay ? m_overlayWidth : m_outputWidth;
        blit.source.h = overlay ? m_overlayHeight : m_outputHeight;
        blit.destination.texture = swapchain;
        blit.destination.x = static_cast<Uint32>(where.x);
        blit.destination.y = static_cast<Uint32>(where.y);
        blit.destination.w = static_cast<Uint32>(where.w);
        blit.destination.h = static_cast<Uint32>(where.h);
        blit.load_op = SDL_GPU_LOADOP_LOAD;
        blit.filter = SDL_GPU_FILTER_LINEAR;
        SDL_BlitGPUTexture(cmd, &blit);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
}

} // namespace satrecomp::video
