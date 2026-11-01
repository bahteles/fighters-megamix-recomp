# Fighters Megamix Recomp

An unofficial native port of *Fighters Megamix* (Sega AM2, Sega Saturn, 1997), made by static
recompilation on [satrecomp](https://github.com/tmcleroy/satrecomp), the toolkit made for the
Fighting Vipers port. Mac first; Linux, SteamOS and Windows follow the toolkit.

> **Early work.** The game starts, its menus work and fights play to the end, all on the toolkit's own
> machine with no emulator. The opening movie does not play yet: wait on the title screen and it
> freezes on its first frame. Press Start on the title screen before it begins. Nothing has been
> tested by hand yet.

**No game files are included.** You need a disc image you made from your own *Fighters Megamix*
(USA) disc. No Saturn BIOS is needed.

This project is not affiliated with, endorsed by, or sponsored by Sega. *Fighters Megamix* and
*Sega Saturn* are trademarks of Sega Corporation.

## What it is

The game's SH-2 code is translated ahead of time into C++ and compiled into the program, where it
runs on satrecomp's own machine: its own video renderer, sound chip, CPUs and hardware, and its own
stand-in for the BIOS. **No emulator is part of the program.** Megamix is built on Fighting Vipers'
engine, and the routines the two games share (file loading, CD music, the controllers) are replaced
by the same C++ in both.

From the toolkit, as in Fighting Vipers: polygons drawn by your graphics card at a higher
resolution, gamepad and keyboard for two players with every button remappable, the launcher, the
in-game menu, save states, and the display options (scaling, picture shape, CRT shaders).
Widescreen and a training mode are planned (see [docs/ROADMAP.md](docs/ROADMAP.md)).

## Building

See [docs/BUILDING.md](docs/BUILDING.md). In short, on a Mac with Xcode's command line tools,
CMake, Ninja, Python 3 and SDL 3:

```sh
cp "/path/to/Fighters Megamix (USA)."* disc/     # your own disc image (.cue and its .bin files)
make && make record ARGS="--frames 6000"        # translate what the game runs
make app                                         # build/fighters_megamix/Fighters Megamix.app
```

[games/fighters_megamix/PROGRESS.md](games/fighters_megamix/PROGRESS.md) says what is native, what
is known about the game, and what is still open.

## License

GPL-3.0-only (see [LICENSE](LICENSE)). Third-party code is listed in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
