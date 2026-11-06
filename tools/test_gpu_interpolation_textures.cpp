// SPDX-License-Identifier: GPL-3.0-only
#include "gpu.h"
#include <algorithm>
#include <cstdio>
#include <vector>
using namespace satrecomp::video;
int main() {
  if (!SDL_Init(SDL_INIT_VIDEO))
    return 2;
  auto *device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
  Gpu gpu;
  if (!device || !gpu.Init(device)) {
    fprintf(stderr, "%s\n", SDL_GetError());
    return 2;
  }
  gpu.SetOutputSize(64, 64);
  gpu.SetScale(1);
  gpu.SetInterpolation(true);
  Frame f;
  f.width = f.height = f.spriteWidth = f.spriteHeight = 64;
  f.spriteMixed = true;
  f.spritePriorities[0] = 1;
  std::vector<uint32_t> layer(4096), back(512, 0xff000000), colors(2048);
  std::vector<uint8_t> mask(4096), vram2(524288);
  std::vector<float> lines(4096);
  for (auto &p : f.layers)
    p = layer.data();
  f.back = back.data();
  f.colors = colors.data();
  f.spriteWindow = mask.data();
  f.floor.vram = vram2.data();
  f.floor.lines = lines.data();
  f.backgroundLines = lines.data();
  f.vram.emplace_back(524288);
  Quad erase{};
  erase.flags = kQuadErase | kQuadFlat;
  erase.corner[1][0] = erase.corner[2][0] = 64;
  erase.corner[2][1] = erase.corner[3][1] = 64;
  erase.clip[2] = erase.clip[3] = 64;
  Quad q{};
  q.width = q.height = q.lines = 16;
  q.texture = 0x60000;
  q.mode = 0x1128;
  q.corner[0][0] = q.corner[3][0] = 8;
  q.corner[1][0] = q.corner[2][0] = 24;
  q.corner[0][1] = q.corner[1][1] = 8;
  q.corner[2][1] = q.corner[3][1] = 24;
  q.clip[2] = q.clip[3] = 64;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      unsigned c = (x == y || x + y == 15) ? 0xffff : 0;
      size_t a = q.texture + 2 * (x + y * 16);
      f.vram[0][a] = c >> 8;
      f.vram[0][a + 1] = c;
    }
  Quad colored = q;
  colored.mode = 0x28;
  colored.texture = 0x61000;
  colored.width = colored.height = 8;
  for (auto &p : colored.corner) {
    p[0] += 26;
    p[1] += 8;
  }
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 8; x++) {
      unsigned c =
          (x + y) % 3 ? 0x8000u | unsigned(x * 4) | unsigned(y * 4) << 5 : 0;
      size_t a = colored.texture + 2 * (x + y * 8);
      f.vram[0][a] = c >> 8;
      f.vram[0][a + 1] = c;
    }
  f.quads = {erase, q, colored};
  f.events = {{Vdp1Event::Kind::Draw, 0, 3, 0}, {Vdp1Event::Kind::Swap}};
  gpu.Draw(f);
  for (size_t i = 1; i < f.quads.size(); i++)
    for (auto &p : f.quads[i].corner)
      p[0] += 2;
  gpu.Draw(f);
  gpu.Draw(f);
  std::vector<uint32_t> native, interpolated;
  bool ok = gpu.Read(native) && gpu.DrawInterpolated(1) &&
            gpu.Read(interpolated) && native.size() == interpolated.size();
  size_t changed = 0, lit = 0;
  if (ok)
    for (size_t i = 0; i < native.size(); i++) {
      changed += native[i] != interpolated[i];
      lit += (native[i] & 0xffffff) != 0;
    }
  printf(
      "native lit pixels=%zu; interpolation endpoint mismatched pixels=%zu\n",
      lit, changed);
  ok = ok && lit > 0 && changed == 0;
  gpu.Shutdown();
  SDL_DestroyGPUDevice(device);
  SDL_Quit();
  return ok ? 0 : 1;
}
