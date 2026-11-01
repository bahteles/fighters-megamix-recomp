# Building

You need **your own disc image** of *Fighters Megamix* (USA, `MK-81073`, V1.004): the `.cue` file and
its `.bin` files, made from a disc you own. Put them in `disc/` (git ignores it) or pass
`DISC=path/to/game.cue`. Nothing from the disc is ever copied into the repository, and no Saturn
BIOS is needed.

## What you need

| System | Packages |
| --- | --- |
| macOS | Xcode Command Line Tools, then `brew install cmake ninja sdl3` |
| Linux | GCC 13 or clang 16 (C++20), CMake 3.28+, Ninja, Python 3, zlib, and SDL 3 (your distribution's package, or built from source; the packages SDL needs are in `tools/release/Dockerfile`) |
| Windows | [MSYS2](https://www.msys2.org), in its MINGW64 shell: `pacman -S make mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-python mingw-w64-x86_64-sdl3 mingw-w64-x86_64-zlib` |

The compiler must be GCC or clang (MinGW-w64 on Windows); MSVC's own compiler is not supported.

## The build

A fresh tree has no translated code, because that is made from your disc. The first build runs
everything in the interpreter; playing it records the code that runs, and the next build has it
translated.

```sh
make                           # build/fighters_megamix/fighters_megamix (interpreter only at first)
make record ARGS="--headless --frames 3000"   # run and record the code that runs
make                           # translate what was recorded to C++ and rebuild
make check                     # interpreter against translated code; two runs alike
make launcher                  # build/fighters_megamix/fighters_megamix-launcher
make run                       # play (windowed), or start the launcher
```

Recording more makes more of the game translated; code the build does not know is run by the
interpreter, so the game always works. [TOOLKIT.md](TOOLKIT.md) ("The loop") has the ways to record
a lot at once: playing, replaying the input movies in `games/fighters_megamix/movies/`, and random
input. `--stats` prints how much ran translated.

Generated files go to `work/fighters_megamix/` and builds to `build/`. Both are ignored by git and
must stay so: they are made from the game.

## Checks

| Command | What it checks | Needs the disc |
| --- | --- | --- |
| `make audit` | Nothing from the game, no emulator's sources, no personal paths in the tracked files | no |
| `make check-toolkit` | The copy of the toolkit (`satrecomp/`) is the one recorded | no |
| `make check-common` | The copy of recomp-common is the one recorded | no |
| `tools/ci_smoke.sh` | The game program and the launcher start (what CI runs) | no |
| `make check` | Interpreter against translated code, state after every frame; two runs alike | yes |
| `make check-rewrites` | Each hand-written routine against the code it replaces, call by call | yes |

`make check` and `make check-rewrites` run `check-toolkit` and `check-common` first.

## Working on the toolkit

`satrecomp/` is a copy of the toolkit's own repository,
[satrecomp](https://github.com/tmcleroy/satrecomp), made by `satrecomp/tools/sync-toolkit.sh`
(run from this repository's top) and never changed here. To change the toolkit, change a clone of
it beside this repository and build and check the game against the clone:

```sh
make SR=../satrecomp                                   # builds to build/fighters_megamix-dev
make SR=../satrecomp check FRAMES=6000 MOVIE=games/fighters_megamix/movies/arcade_fight.txt
make SR=../satrecomp check-rewrites
```

The check scripts follow the build's folder (`B=build/fighters_megamix-dev`). Then commit in the
toolkit's repository, run `satrecomp/tools/sync-toolkit.sh` here, run the checks again on the copy,
and commit it.

## The reference build (development only)

Each part of the port's machine was developed against the [Ymir](https://github.com/StrikerX3/Ymir)
emulator, and can still be compared with it. Ymir is not in this repository and not in the game, and
neither is the patch that adds the comparison hooks to it (a patch quotes the source it changes, and
this repository holds no emulator's code). The maintainer has the patch; `SATRECOMP_REFERENCE` names
the folder it is in (by default `../satrecomp-reference`).

```sh
make setup                     # fetch Ymir into satrecomp/third_party/ (ignored by git) and patch it
make ref                       # build/fighters_megamix-ref/fighters_megamix-ref
make verify                    # translated code against Ymir's interpreter
```

See [satrecomp/docs/VERIFYING.md](../satrecomp/docs/VERIFYING.md). You don't need any of this to
build, play or change the game.

## Release builds

Release builds contain code translated from the game, so they are made by someone with the disc.
All write to `build/release/`, and each checks its archive for game data, emulator names and
personal paths before it is packed (`tools/release/check_archive.sh`).

| Script | Run on | Makes |
| --- | --- | --- |
| `tools/release/macos.sh` | macOS on Apple silicon | `FightersMegamix-macos.zip`: `Fighters Megamix.app` for Apple silicon and Intel (macOS 13+, SDL 3 static, signed ad hoc) |
| `tools/release/linux.sh` | x86_64 Linux, e.g. the container of `tools/release/Dockerfile` | `FightersMegamix-linux-x86_64.tar.gz` (SDL 3 and the C++ library static; also for SteamOS) |
| `tools/release/windows.sh` | x86_64 Linux with MinGW-w64 (cross-compile), the same container | `FightersMegamix-windows-x64.zip` (with `SDL3.dll`) |

`tools/release/all.sh` runs the three on an Apple silicon Mac with Docker and writes
`SHA256SUMS.txt`. [RELEASING.md](RELEASING.md) has the whole process.
