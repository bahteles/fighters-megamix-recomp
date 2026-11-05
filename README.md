> **Community fork maintained by bahteles.** Based on Tommy McLeroy's Fighters Megamix recompilation.
> **Features:** full 16:9 support, SFX bug fixes, optional HD texture pack support, and an in-game model Gallery.
> Texture packs are supplied separately and are not included in this repository. See [COMMUNITY.md](COMMUNITY.md).

# Fighters Megamix Recomp

An unofficial native PC port of *Fighters Megamix* (Sega AM2, Sega Saturn, 1997) for macOS, Linux, SteamOS and
Windows, made by static recompilation on [satrecomp](https://github.com/tmcleroy/satrecomp), the toolkit made for
the Fighting Vipers port.

> **Early work.** The game starts, its menus work and fights play to the end, all on the toolkit's own machine with
> no emulator. The opening movie plays and can be skipped. Nothing has been tested by hand yet.

![A fight in 16:9 at the window's resolution](docs/screenshots/widescreen.jpg)

**No game files are included.** You need a disc image you made from your own *Fighters Megamix* (USA) disc. No
Saturn BIOS is needed.

The game's SH-2 code is translated ahead of time into C++ and compiled into the program, where it runs on
satrecomp's own machine: its own video renderer, sound chip, CPUs and hardware, and its own stand-in for the BIOS.
**No emulator is part of the program.** Megamix is built on Fighting Vipers' engine, and the routines the two games
share (file loading, CD music, the controllers) are replaced by the same C++ in both.

## The picture

- **High resolution**: polygons drawn by your graphics card at the window's resolution, not the Saturn's.
- **Full 16:9 support**: widescreen fights, menus, victory screens and credits, with aspect-aware Gallery presentation.
- **The display your way**: sharp, whole-pixel or smooth scaling, 4:3 or square pixels, the window's size, and
  RetroArch's CRT and scanline shaders.

| | |
| --- | --- |
| ![A fight at the window's resolution, 4:3](docs/screenshots/fight.jpg) | ![The character select](docs/screenshots/select.jpg) |

**[More about the picture →](docs/PICTURE.md)**

## Gallery mode

Browse the 32 fighters in the in-game **Gallery**, with rotation, zoom, original color variants,
and full or broken armor where available. Optional HD textures and CRT filters are supported.

![Gallery mode showing Bahn in full armor with character, view, armor and color controls](docs/screenshots/gallery.png)

Gallery models, interface artwork and the optional texture pack are local resources and are not
included in this repository. See [Gallery setup and controls](docs/GALLERY.md).

## Everything else

- **Save states**: nine slots, from the menu or F5 / F9 ([keys](docs/LAUNCHER.md#keys-while-playing)).
- **Run-ahead**: what you press seen a frame or two sooner, the game running exactly as without it
  ([more](docs/LAUNCHER.md#run-ahead)).
- **Unlock everything**: every character, course and extra option without playing for them, your own save kept as
  it is ([more](docs/LAUNCHER.md#unlock-everything)).
- **Controls**: a gamepad or the keyboard for two players, every button changeable, and buttons pressed together on
  one key ([more](docs/LAUNCHER.md#controllers)).
- **Updates** in the launcher, and a **portable mode** that keeps everything beside the program
  ([the launcher](docs/LAUNCHER.md)).
- **A training mode** is planned, like Fighting Vipers' ([docs/ROADMAP.md](docs/ROADMAP.md)).

## Installing

Download the file for your system from the [releases page](../../releases), then:

| System | File | Then |
| --- | --- | --- |
| macOS 13 or later, Apple silicon or Intel | `FightersMegamix-macos.zip` | Unzip and open **Fighters Megamix.app**. It isn't notarized: the first time, right-click it and choose **Open** (or allow it under System Settings, Privacy & Security). |
| Linux x86_64, SteamOS (Steam Deck, Steam Machine) | `FightersMegamix-linux-x86_64.tar.gz` | Unpack and run `fighters_megamix-launcher`. On SteamOS, do this in Desktop Mode, and use the launcher's **Add to Steam** to have it in Game Mode. |
| Linux ARM64 (aarch64) | `FightersMegamix-linux-arm64.tar.gz` | Unpack and run `fighters_megamix-launcher`. |
| Windows 10 or later, x64 | `FightersMegamix-windows-x64.zip` | Unzip and run `fighters_megamix-launcher.exe`. Windows warns about an unknown publisher the first time: choose **More info**, then **Run anyway**. |

In the launcher, drop your disc image's `.cue` file on the window (or choose it), choose **Features** and
**Controllers** as you like (or leave them), and **Play**. The launcher checks that the image is the release this
port is made from (USA, `MK-81073`, V1.004) and remembers where it is; it never copies, moves or changes it.

The disc image must be one you made from a disc you own. This project can't help you find game files, and requests
for them are closed. Where your settings and saves are kept: [docs/LAUNCHER.md](docs/LAUNCHER.md#your-files).

## Controls

Keyboard: arrows, Z / X / C for guard, punch and kick, Return for Start; on a gamepad, the d-pad or left stick and
the lower buttons. While playing, **Esc**, **F1** or the gamepad's Back button opens the menu (its "Quit game" row
is how you leave), and **F11** switches full screen. [docs/LAUNCHER.md](docs/LAUNCHER.md#controllers) has the whole
layout and every key.

## Status

| | |
| --- | --- |
| The game | Boot, title, mode select, character select and arcade fights (rounds, replay, continue, game over) run; the opening movie plays to its end and can be skipped. Seen in screenshots taken with no window, not yet played, heard or watched in a window by a person. |
| Platforms | macOS (Apple silicon, and Intel under Rosetta), Linux x86_64 (also a Steam Machine on SteamOS) and Windows x64 (under Wine): the release downloads replay a recorded fight to the same state as the Mac. |

[games/fighters_megamix/PROGRESS.md](games/fighters_megamix/PROGRESS.md) says what is native, what is known about
the game, and what is still open; [docs/ROADMAP.md](docs/ROADMAP.md) has what is planned.

## Building from source

See [docs/BUILDING.md](docs/BUILDING.md). In short, on a Mac with Xcode's command line tools, CMake, Ninja,
Python 3 and SDL 3:

```sh
cp "/path/to/Fighters Megamix (USA)."* disc/     # your own disc image (.cue and its .bin files)
make && make record ARGS="--frames 6000"        # translate what the game runs
make app                                         # build/fighters_megamix/Fighters Megamix.app
```

[docs/TOOLKIT.md](docs/TOOLKIT.md) explains how the port is made.

## Feedback and contributing

Please [open an issue](../../issues) for bugs, crashes, glitches or ideas, with your system, your controller and
which features were on. [CONTRIBUTING.md](CONTRIBUTING.md) has the rules for code, the first of which is that
nothing from the game ever goes in the repository.

## License

GPL-3.0-only (see [LICENSE](LICENSE)). Third-party code is listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

This project is not affiliated with, endorsed by, or sponsored by Sega. *Fighters Megamix* and *Sega Saturn* are
trademarks of Sega Corporation.
