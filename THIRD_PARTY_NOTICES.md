# Third-party notices

Fighters Megamix Recomp (the `satrecomp` toolkit and the game's own sources) is licensed under the
GNU General Public License v3.0 (see `LICENSE`). The licenses of everything below are compatible
with it.

## In this repository

### Ymir (GPL-3.0)

[Ymir](https://github.com/StrikerX3/Ymir), a Sega Saturn emulator by StrikerX3 and contributors,
is the reference this port's machine was developed against, and is not part of the port: no Ymir
source is compiled or linked into the game, and the emulator is not in this repository.

- **The machine's own models** (`satrecomp/machine/`, `satrecomp/video/`, `satrecomp/sound/`,
  `satrecomp/runtime/backup.*`) were written for this project, with Ymir's source open beside
  them and each checked against Ymir state for state. Register layouts, timing tables and
  instruction timings were read from it, and many names of registers and fields, and in places a
  line or an expression, are the same as Ymir's. Where the law would call any of that derived
  from Ymir, it is used under the GPL-3.0, and this is its notice.
- **The VDP1's counters** (`satrecomp/video/counters.h`: how the chip steps along lines, quad edges,
  textures and shading) are this project's code. The counting rules are the hardware's, and were
  learnt from Ymir's `vdp1_steppers.hpp`, a copy of which this file replaced; the two were compared
  on billions of cases and give the same results.
- **The development reference build** (`make setup`, `make ref`) fetches Ymir from its own repository
  and applies a patch that is kept outside this one. The files here that are compiled only into
  that build (`video/ymir_adapter.*`, `sound/generator.*`, `runtime/m68k_*`) are this project's,
  written against Ymir's interfaces.

### wiiparty-recomp (GPL-3.0-or-later) and recomp-common (GPL-3.0)

`satrecomp/recomp-common/` is a copy of [recomp-common](https://github.com/tmcleroy/recomp-common)
(the in-game menu, the settings rules and the launcher; this author's, GPL-3.0), made by
`satrecomp/tools/sync-common.sh` and never changed here. Its launcher descends from the launcher of
[wiiparty-recomp](https://github.com/arelkair/wiiparty-recomp) by arelkair (GPL-3.0-or-later):
`launcher/src/ui.cpp`, `ui.h` and `embedded.h` are that project's files nearly unchanged and are
marked GPL-3.0-or-later; `main.cpp`, parts of `app.cpp`, `game_process.cpp` and `embed_fonts.cmake`
follow it. recomp-common's own `THIRD_PARTY_NOTICES.md` has the table, file by file.

The 5x7 dot-matrix font of the menu (`satrecomp/recomp-common/text.h`) is the generic character
set of LCD and LED displays, typed in for recomp-common.

## Downloaded at build time and included in release builds

None of these is stored in this repository.

| Component | License | Used for | In the release |
| --- | --- | --- | --- |
| [SDL 3](https://libsdl.org) 3.4.16 | zlib. Copyright (C) 1997-2026 Sam Lantinga | Window, input, audio, GPU (game and launcher) | Linked statically (macOS, Linux); `SDL3.dll` (Windows) |
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9b | MIT. Copyright (c) 2014-2026 Omar Cornut | The launcher's interface | Compiled into the launcher |
| [Inter](https://rsms.me/inter/) 4.1 | SIL Open Font License 1.1. Copyright (c) 2016 The Inter Project Authors | The launcher's font | Compiled into the launcher |
| [librashader](https://github.com/SnowflakePowered/librashader) 0.12.1 | MPL-2.0 or GPL-3.0 (used here under GPL-3.0). Copyright (c) chyyran and its contributors. Its C header, `satrecomp/port/librashader.h`, is in this repository: MIT | The Display page's shaders | `librashader.dylib` in the app's Frameworks, `librashader.so` / `librashader.dll` beside the programs, built from source (`satrecomp/tools/librashader.sh`) with the Rust crates its `Cargo.lock` names; its licenses in `licenses/` |
| [slang-shaders](https://github.com/libretro/slang-shaders) (commit `2b0530b`) | Each file its own, stated at its top: crt-geom-mini (after cgwg's crt-geom), CRT Royale (TroggleMonkey), CRT Guest (guest(r), Dr. Venom) and zfast-crt, CRT Light (Greg Hogan) GPL-2.0-or-later; CRT Lottes (Timothy Lottes), scanline-fract (hunterk) and sharp-bilinear-scanlines (rsn8887) public domain; xBRZ (Hyllian, Zenju) and ScaleFX (Sp00kyFox) and the blur and gamma includes MIT; the stock pass (libretro), ossc (DariusG) and integer-scaling-scanlines with no notice, and crt-geom-mini's linearize pass none either | The presets the Display page's Shader names | In `shaders/` (the app's `Resources/shaders`), with the files they use (`satrecomp/tools/shaders.py`) |
| [zlib](https://zlib.net) | zlib. Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler | Screenshots (PNG) | The system's (macOS, Linux); linked statically (Windows) |
| GCC runtime libraries (libstdc++, libgcc, winpthreads) | GPL-3.0 with the GCC Runtime Library Exception; MIT and BSD (winpthreads) | The C++ runtime | Linked statically (Linux, Windows) |

Each download is checked against its published SHA-256. Release archives carry the license texts of
SDL 3, Dear ImGui and Inter in a `licenses/` folder, beside this file and the GPL-3.0.

## Tools used to build (not included in builds)

Python, CMake, Ninja, a C++ compiler; glslangValidator and SPIRV-Cross to regenerate
`satrecomp/video/shaders.inc` from the shaders in `satrecomp/video/shaders/` (which are this
project's).

## What is not included

Nothing from *Fighters Megamix* or the Saturn itself is in this repository: no game code, data,
graphics, sound or text, no BIOS, and no files from Sega's development kits. The port provides the
BIOS's services with code of its own (`satrecomp/runtime/hle_bios.cpp`), written from watching what
a game asks of them; the comments there use the names by which the backup library's functions are
publicly known. Release builds contain code translated from the game by the recompiler; the game's
data is read from each player's own disc image.

Code added later from another project must have a license compatible with the GPL-3.0 and be listed
here, with its notice kept in the file. `tools/audit.sh` fails if a file marked as another
project's is not one of those listed above, and on any patch file or emulator's source.
