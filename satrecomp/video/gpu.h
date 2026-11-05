// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of satrecomp: https://github.com/tmcleroy/satrecomp
// The picture drawn by the GPU (SDL_GPU: Metal, Vulkan, Direct3D 12) from a frame as the renderer hands it
// over (frame.h): the VDP1's quads into a target several times the original's resolution, then that and the
// VDP2's layers put together by priority.
#pragma once

#include "frame.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace satrecomp::video {

// Where in a `width` x `height` window a picture `columns` x `lines` goes (the Display page, Gpu::SetFit):
// `aspect` its own shape (4:3, or wider in widescreen); `scaling` 0 as large as fits, 1 as tall as a whole
// number of its lines (a field's, for an interlaced picture), 2 as 0; `shape` 0 its own, 1 square pixels, 2 the
// whole window
SDL_FRect FitPicture(int width, int height, float columns, float lines, float aspect, int scaling, int shape);

class Gpu {
public:
    bool Init(SDL_GPUDevice *device);
    void Shutdown();

    // The size in pixels the picture will be shown at; the VDP1's target is sized to match
    void SetOutputSize(uint32_t width, uint32_t height);
    // How many times the original's resolution the VDP1's quads are drawn at (0: as the output size needs)
    // The picture's shape, width / height (4:3 unless widescreen)
    void SetAspect(float aspect) {
        m_aspect = aspect;
    }
    float Aspect() const {
        return m_aspect;
    }
    void SetScale(uint32_t scale) {
        m_fixedScale = scale;
    }

    // Does a frame's VDP1 work and makes the picture, in Output()
    void Draw(const Frame &frame);
    void RedrawFrozenFrame();
    // Re-render compatible polygon positions between two source pictures, without advancing the machine.
    bool DrawInterpolated(float fraction);
    void SetInterpolation(bool enabled);
    uint64_t InterpolatedPictures() const { return m_interpolatedPictures; }
    void SetUiContours(bool enabled) { m_uiContours = enabled; }
    void SetHdTexturePack(bool enabled);
    bool HdTexturePackEnabled() const { return m_hdEnabled; }
    // The modal model viewer shares the selected pack and exact original-pixel matching.
    bool PreviewHdTexture(int width, int height, const uint32_t *original, std::vector<uint32_t> &replacement);
    void EndTexturePreview() { std::string().swap(m_hdPreview); }
    void SetHdTexturePackPath(const std::string &path) { m_hdPackPath = path; }
    void SetTextureUpscale(bool enabled) { m_textureUpscale = enabled; }
    // Run-ahead (port/main.cpp): the VDP1's two framebuffers as drawn here are kept before frames that will
    // be taken back, and put back after them (so the next frame draws on what it would have without them).
    // The picture made meanwhile stays: it is the one shown.
    void KeepSprites();
    void PutSpritesBack();
    SDL_GPUTexture *Output() const {
        return m_output;
    }
    uint32_t OutputWidth() const {
        return m_outputWidth;
    }
    uint32_t OutputHeight() const {
        return m_outputHeight;
    }
    // The picture's pixels (RGBA), for screenshots and tests
    bool Read(std::vector<uint32_t> &pixels);
    // The VDP1 framebuffer on display as drawn here (its 16-bit values), for tests
    bool ReadSprites(std::vector<uint16_t> &words, uint32_t &width, uint32_t &height);
    // How Present fits the picture to the window (the Display page, port/present.h): `scaling` 0 sharp (as large
    // as fits, drawn at that size), 1 whole pixels (as tall as a whole number of the picture's lines fits), 2
    // smooth (with a fixed resolution: drawn at it and softened up to the size); `shape` 0 the picture's own
    // (4:3, or wider in widescreen), 1 square pixels, 2 the whole window
    void SetFit(int scaling, int shape) {
        m_scaling = scaling;
        m_shape = shape;
    }
    // The picture drawn at the Saturn's own size (its lines and columns, wider in widescreen), whatever the
    // window: for a shader, which draws it up to the window itself (port/shading.h)
    void SetNative(bool native) {
        m_native = native;
    }
    // The picture's own size: the frame's columns and lines (0 before the first frame)
    uint32_t PictureWidth() const {
        return m_layerWidth;
    }
    uint32_t PictureHeight() const {
        return m_layerHeight;
    }
    // Where in a window of that size the picture goes, as SetFit says
    SDL_FRect Fit(int width, int height) const;
    // Sizes the output for the next frame as Present will show it in a window of that size (SetFit, SetNative)
    void SizeFor(int width, int height);
    // Shows the picture in a window, as SetFit says
    void Present(SDL_Window *window);
    // A picture to show in place of the one drawn (the game's with a menu over it, while the game stands
    // still): 32-bit pixels, red in the low byte. ClearOverlay: the drawn picture again.
    void SetOverlay(const uint32_t *pixels, uint32_t width, uint32_t height);
    void ClearOverlay() {
        m_overlayShown = false;
    }
    // A picture with alpha shown over the drawn one, stretched to it (the readout, port/hud.h): 32-bit pixels,
    // red in the low byte. It is blended into the output when the output is shown (Present) or read with it
    // (ReadShown), so Read gives the game's picture alone.
    void SetHudLayer(const uint32_t *pixels, uint32_t width, uint32_t height);
    void ClearHudLayer() {
        m_hudChanged = m_hudChanged || m_hudShown;
        m_hudShown = false;
    }
    bool ReadShown(std::vector<uint32_t> &pixels);

private:
    void ApplyHud();
    SDL_GPUGraphicsPipeline *m_hudPipeline = nullptr;
    SDL_GPUSampler *m_linear = nullptr;
    SDL_GPUTexture *m_hud = nullptr;
    uint32_t m_hudWidth = 0, m_hudHeight = 0;
    bool m_hudShown = false, m_hudApplied = false; // (applied: to the output as it is now)
    // The output as it was before the readout went onto it, kept while a readout shows: when the readout
    // changes and the picture does not (the game stands still: a note says it is stopped), the picture is
    // put back and the new readout goes on
    SDL_GPUTexture *m_plain = nullptr;
    uint32_t m_plainWidth = 0, m_plainHeight = 0;
    bool m_hudChanged = false;
    SDL_GPUDevice *m_device = nullptr;
    SDL_GPUGraphicsPipeline *m_quadPipeline = nullptr, *m_composePipeline = nullptr;
    SDL_GPUSampler *m_nearest = nullptr;
    SDL_GPUTexture *m_sprites[2] = {}; // the VDP1's two framebuffers: [0] on display, [1] drawn to
    SDL_GPUTexture *m_keptSprites[2] = {}; // (run-ahead's copy of them, and the size it is of)
    uint32_t m_keptWidth = 0, m_keptHeight = 0, m_keptScale = 0;
    void CopySprites(SDL_GPUTexture *const from[2], SDL_GPUTexture *const to[2]);
    SDL_GPUTexture *m_vram = nullptr, *m_colors = nullptr, *m_back = nullptr, *m_window = nullptr;
    SDL_GPUTexture *m_vram2 = nullptr, *m_floorLines = nullptr; // for the rotation screen drawn here
    SDL_GPUTexture *m_layers[5] = {};
    SDL_GPUTexture *m_output = nullptr;
    SDL_GPUBuffer *m_vertices = nullptr;
    SDL_GPUTransferBuffer *m_transfer = nullptr, *m_download = nullptr;
    uint32_t m_vertexCapacity = 0, m_transferCapacity = 0, m_downloadCapacity = 0;
    uint32_t m_spriteWidth = 0, m_spriteHeight = 0, m_scale = 0, m_fixedScale = 0;
    uint32_t m_layerWidth = 0, m_layerHeight = 0;
    uint32_t m_outputWidth = 0, m_outputHeight = 0, m_wantedWidth = 640, m_wantedHeight = 480;
    struct HdTexture {
        uint16_t width, height;
        uint32_t offset;
        std::vector<uint8_t> original;
        uint32_t previewOffset = 0;
    };
    std::vector<HdTexture> m_hdTextures;
    std::string m_hdPackPath;
    std::string m_hdPreview; // only retained while the modal viewer is open
    SDL_GPUTexture *m_hdAtlas = nullptr;
    struct HdText {
        int32_t leftFromCenter;
        uint32_t top, width, height, layer, scale, offset;
        std::vector<uint32_t> original;
    };
    std::vector<HdText> m_hdText;
    uint32_t m_hdAirportOffset = 0;
    uint32_t m_hdAirportCrc[3]{};
    struct HdBackground { uint32_t offset, map, cells, cellBytes, palette, crc[3]; };
    std::vector<HdBackground> m_hdBackgrounds;
    SDL_GPUTexture *m_backgroundLines = nullptr;
    struct StagePage {
        uint32_t bank, bytes, cellsCrc, mapCrc, paletteCount, paletteCrc, compressed, crc;
        uint32_t width, phase, decodedBytes;
        uint64_t fileOffset;
    };
    std::vector<StagePage> m_stagePages;
    std::vector<int> m_activeStagePages;
    std::vector<uint32_t> m_stageOffsets, m_stageQuadOffsets;
    SDL_GPUTexture *m_stageAtlas = nullptr;
    uint32_t m_stageRepeatOffset = 0, m_stageRepeatTile = 0;
    void IndexStagePages();
    bool LoadStagePages(const std::vector<int> &pages);
    void StagePageOffsets(const Frame &frame, uint32_t (&floor)[64], uint32_t (&background)[32]);
    Frame m_frozenFrame, m_pendingSprites, m_displaySprites;
    std::vector<uint32_t> m_frozenLayers[5], m_frozenBack, m_frozenColors;
    std::vector<uint8_t> m_frozenWindow, m_frozenVram2;
    std::vector<float> m_frozenFloorLines, m_frozenBackgroundLines;
    bool m_redrawing = false;
    bool m_interpolationEnabled = false;
    std::vector<Quad> m_previousQuads;
    uint32_t m_pictureAge = 0, m_pictureInterval = 1;
    uint64_t m_interpolatedPictures = 0;
    bool MatchesHdText(const Frame &frame, const HdText &text) const;
    bool m_hdEnabled = false, m_hdAttempted = false;
    bool LoadHdTexturePack();
    struct ScreenArt {
        uint32_t layer,sourceWidth,sourceHeight,kind,crc;
        int32_t left,top,width,height;
        uint32_t columns,lines;
        std::string member;
    };
    std::vector<ScreenArt> m_screenArt;
    SDL_GPUTexture *m_screenTexture=nullptr;
    bool m_screenAttempted=false;
    int m_screenUploaded=-1;
    void UpdateScreenArt(const Frame &frame,int32_t (&rect)[4],int32_t (&meta)[4]);
    struct RecordGlyph { uint32_t width,crc; std::vector<uint8_t> pixels; };
    std::vector<RecordGlyph> m_recordGlyphs;
    std::vector<uint8_t> m_recordsHeader;
    bool m_recordsAttempted=false;
    uint32_t m_recordsCrc=0;
    bool UploadScreenPixels(const void *pixels,uint32_t width,uint32_t height);
    void UpdateRecordArt(const Frame &frame,int32_t (&rect)[4],int32_t (&meta)[4]);
    bool m_uiContours = false;
    bool m_textureUpscale = false;
    float m_aspect = 4.0f / 3.0f;
    int m_scaling = 0, m_shape = 0;
    bool m_native = false;
    SDL_GPUTexture *m_overlay = nullptr;
    uint32_t m_overlayWidth = 0, m_overlayHeight = 0;
    bool m_overlayShown = false;

    SDL_GPUShader *Shader(SDL_GPUShaderStage stage, const unsigned char *spirv, size_t spirvSize, const char *msl,
                          uint32_t samplers, uint32_t uniforms);
    SDL_GPUTexture *Texture(SDL_GPUTextureFormat format, uint32_t width, uint32_t height, bool target);
    void Release(SDL_GPUTexture *&texture);
    bool Prepare(const Frame &frame);
    bool Download(SDL_GPUTexture *texture, uint32_t width, uint32_t height, uint32_t bytesPerPixel, void *out);
};

} // namespace satrecomp::video
