# satrecomp: how the port is made

Native ports of Sega Saturn games by static recompilation: the game's SH-2 code is translated to
C++ ahead of time and compiled into the port, where it runs on a small machine of satrecomp's own:
its video renderer, sound chip, CPUs and hardware registers. **No emulator is part of the game
binary.** An emulator (Ymir) is used only during development, as the reference each piece is
checked against. **No game data is included: you need your own disc image.** No Saturn BIOS image
is needed.

| Path | What |
| --- | --- |
| `satrecomp/` | The reusable toolkit, a copy of [its own repository](https://github.com/tmcleroy/satrecomp)'s made by `satrecomp/tools/sync-toolkit.sh` (never changed here; its own notes are [satrecomp/README.md](../satrecomp/README.md) and `satrecomp/docs/`): runtime (block dispatcher, recorder, start-up code and BIOS services), code generator, the machine (`machine/`: CPUs, bus, interrupts, timing; [docs/MACHINE.md](../satrecomp/docs/MACHINE.md)), video renderer (`video/`: software and SDL_GPU), sound chip and sound CPU (`sound/`), port frontend (SDL3), the code shared with the other recomp ports (`recomp-common/`: the in-game menu, the settings rules and the launcher, a copy of [recomp-common](https://github.com/tmcleroy/recomp-common) made by `tools/sync-common.sh`; never changed here), launcher, checking tools |
| `satrecomp/third_party/Ymir` | Not in the repository. [Ymir](https://github.com/StrikerX3/Ymir) (GPL-3), fetched there by `make setup` for development only (`make ref`): the reference machine, with a patch applied that is kept outside this repository. Not needed to build or play the game |
| `games/<id>/` | One game: `game.cfg`, its own sources `src/` (routines replaced by hand-written C++), recorded input `movies/`, notes |

Generated files go to `work/<id>/` (block database, generated C++, saves) and builds to
`build/<id>/`. Both are gitignored, because they're derived from the game.

## Games

| Game | Status |
| --- | --- |
| [Fighters Megamix](../games/fighters_megamix/PROGRESS.md) (Sega AM2, 1997) | Boots without a BIOS; menus, fights and the opening movie run on satrecomp's own machine with no emulator in the binary: translated SH-2 code, own renderer, own sound chip and sound CPU |
| [Fighting Vipers](https://github.com/tmcleroy/fighting-vipers-recomp) (Sega AM2, 1996) | The first game on the toolkit, with widescreen and a training mode |

## Building

See [BUILDING.md](BUILDING.md).

## How it works

- **Blocks.** While the game runs, every straight run of SH-2 instructions it reaches (up to an
  unconditional branch) is recorded in the block database (`work/<id>/blocks.txt`): address and code
  bytes. Code the build doesn't know yet is run by the interpreter, so the game always works.
- **Static discovery.** While recording, the code each block leads to (fixed branches, calls, calls
  through literals) is followed through memory and added to the database, so code no run reached is
  translated as well.
- **Native code.** `satrecomp/tools/recomp.py` with `sh2emit.py` writes
  a C++ function per block: plain code on the registers (`R[3] += R[4];`), work RAM and ROM accessed
  directly, everything else through the hardware layer. Blocks run to their end and time is counted
  in nominal cycles per instruction. Blocks joined by branches with fixed targets become one function
  with gotos, so loops and conditions stay in native code; loops that only wait are skipped ahead.
- **Dispatch.** A block is found by address and verified by its bytes, so code that is replaced in
  RAM (programs loaded over one another) never runs a stale translation.
- **The machine.** What the game's code runs on is satrecomp's own (`satrecomp/machine/`,
  [docs/MACHINE.md](../satrecomp/docs/MACHINE.md)): two SH-2s whose state the translated code works on
  directly, with an interpreter for code the build has no translation for; the bus; the interrupt
  controller, timers and DMA; the video chips' registers and timing; and what the game still asks
  of the CD drive. It is not a model of the hardware's timing: it does what this code needs.
- **Native BIOS.** The port starts the disc itself and provides the BIOS's services (interrupt entry
  and exit, system calls, slave CPU start, backup memory library with saves in the console's
  format) as C++ routines (`satrecomp/runtime/hle_bios.cpp`).
- **Picture and sound.** The renderer (`video/`, [docs/VIDEO.md](../satrecomp/docs/VIDEO.md)) and the
  sound chip with its 68000 (`sound/`, [docs/SOUND.md](../satrecomp/docs/SOUND.md)) are satrecomp's
  own too; the sound driver runs in the 68000 interpreter.
- **Lifted routines.** A game's own sources (`games/<id>/src`) can replace routines of the game with
  hand-written C++, by address and code hash (`satrecomp/runtime/lift.h`). Fighters Megamix' file
  loader, file library, CD music calls and pad reading are: the same routines as Fighting Vipers', so the
  same C++ (`satrecomp/runtime/shared_lifts.h`); files come straight from the disc image.
- **Readable code.** Where a feature needs the game's rules, the routine is rewritten as ordinary C++
  with names and checked call by call against the code it replaces (`SATRECOMP_LIFT_CHECK=1`,
  [satrecomp/docs/READABLE.md](../satrecomp/docs/READABLE.md)). Fighters Megamix has none yet; Fighting
  Vipers' rewrites of the same engine are the model.
- **Sync points.** Games rely on the two SH-2s running side by side. Where one must give the other
  time, `game.cfg` names the address (`sync_points`).
- **Checking.** On its own machine (`make check`): everything run by the SH-2 interpreter must give
  the same machine state after every frame as the translated code, and two runs must be the same.
  On the reference build (`make ref`, with Ymir): the translated code against Ymir's interpreter
  frame by frame (`make verify`); the renderer against Ymir's pixel by pixel
  (`SATRECOMP_VIDEO=check`); the sound chip against Ymir's state by state (`SATRECOMP_SOUND=check`);
  the 68000 against Ymir's instruction by instruction (`SATRECOMP_M68K=check`).
  See [satrecomp/docs/VERIFYING.md](../satrecomp/docs/VERIFYING.md).

## The loop

```sh
make record                                        # play; new blocks are added to the database
build/<id>/<id> ... --headless --frames 24000 --auto 1:1100 \
    --record movie.txt --record-blocks blocks_1.txt   # automatic exploration (random input)
satrecomp/tools/merge_blocks.sh work/<id>/blocks.txt blocks_*.txt
make && make check MOVIE=movie.txt FRAMES=8000         # a movie replays only on the machine it was recorded on
```

`--stats` prints how many block runs were recompiled and how many fell back to the interpreter.

## Adding a game

1. Create `games/<id>/game.cfg` (name, app id, the disc's product number and version).
2. `make GAME=<id>`, then record blocks by playing and with explorers, rebuild, verify.

## The launcher and the settings

The launcher (`make launcher`) is the one the recomp ports share: `satrecomp/recomp-common/launcher/`, a copy of
[recomp-common](https://github.com/tmcleroy/recomp-common)'s made by `tools/sync-common.sh` and never changed
here. What is this toolkit's is `satrecomp/launcher/src/description.cpp`: the words for a disc image, the check of the player's (the disc's header), what the game is started
with, the Display switches, and the buttons that can be chosen (`port/bindings.h`).
The launcher and the game read and write `settings.ini` by the same rules (`recomp-common/settings.h`): the
`[features]` section (a preset, and a value for each feature the player set) and the `[keyboard1]` /
`[gamepad1]` sections (which key and which gamepad button does what). Changing one feature, in the launcher
or in the game's menu, leaves every other line of the file as it was.
Each player's keys and gamepad buttons can be chosen on the launcher's Controllers page (the usual layout is
as before: arrows, Return, Z X C, A S D, Q E; a gamepad's lower buttons A B C, shoulders and top X Y Z,
triggers L R). The game reads them once when its window opens. The launcher starts the game in a window
unless "Full screen" is switched on (it used to be on unless switched off).

The launcher's pages can be pictured with no window, to compare before and after a change:

```sh
SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software FIGHTERS_MEGAMIX_HOME=/tmp/home build/fighters_megamix/fighters_megamix-launcher --screenshot /tmp/page
```

Where the launcher's look comes from, and the licenses of what its build downloads, are in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
