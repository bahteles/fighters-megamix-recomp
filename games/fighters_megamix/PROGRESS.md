# Fighters Megamix: progress

Disc: *Fighters Megamix* (USA), MK-81073 V1.004 (1997-04-11). The main program is `1ST_READ.DAT`
(571,480 bytes, loaded at 0x06004000); everything else is in three big files (`FMM_DATA.DAT` 27.7 MB,
`MOV_DATA.DAT` 23 MB of movies, `SEL_DATA.DAT`) and 41 CD audio tracks.

## Status (2026-11-01)

| | |
| --- | --- |
| Boot, title, mode select, character select | runs, on the own machine (no BIOS image, no emulator) |
| Arcade fights (rounds, replay, continue, game over) | runs; seen in headless screenshots only |
| Opening movie (title screen left alone ~11 s) | **freezes on its first frame** (see "Open problems") |
| Skipping the movie with Start | **breaks** (slave CPU) when started without a BIOS image |
| Translated code | 15,520 blocks recorded (explorers + `movies/arcade_fight.txt`); 100% of the arcade movie's blocks run translated |
| `make check` | passes: `FRAMES=6000 MOVIE=games/fighters_megamix/movies/arcade_fight.txt` |
| Speed | ~195 fps headless (software renderer), one thread, M-series Mac |
| Platforms (2026-11-01) | macOS arm64 and x86_64 (Rosetta), Linux x86_64 (container and a Steam Machine, SteamOS) and Windows x64 (Wine): the release downloads give the Mac's state hashes for 3,000 frames of `movies/arcade_fight.txt`; the GPU renderer on the Steam Machine's Vulkan draws what the Mac's Metal does |
| Hand play, sound, window | **not tested by a person**: no one has played it, listened to it or seen it in a window |

## What it shares with Fighting Vipers

Megamix is built on Fighting Vipers' engine. Byte for byte the same (searched by the bytes of the code
each Fighting Vipers lift hashes; the constant pools differ), and replaced by the toolkit's shared C++
(`satrecomp/runtime/shared_lifts.h`, registered in `src/game.cpp`):

| Routine | Megamix | Fighting Vipers |
| --- | --- | --- |
| AM2 LoadFile | 0602DE3C | 06029B44 |
| AM2 ReadPads | 0602F168 | 0602BA24 |
| GFS Open / Start / Poll / Close | 060483BC / 06048664 / 060489D8 / 0604894C | 06044D78 / 06045020 / 06045394 / 06045308 |
| GFS Fread | 06049270 | 06045C2C |
| CdPlay / CdSeek / GetCurStat | 06047F50 / 06048014 / 060479C8 | 0604490C / 060449D0 / 06044384 |
| PER SendCommand | 0605C458; the movie player's copy 060D4510 | 06058934 |

Also the same code, not replaced yet: TakeDamage (0x06012CE8 / 0x06012CEC, FV 0x060117D0 / D4),
InputTask (0x06016246, FV 0x06014C06), CountTimeTaken (0x06022B54, FV 0x06024D58). The start of
FightersTask (0x06010E60), RoundClock (0x06022AF8), InputStep (0x0601636C) and JudgeTimeUp
(0x06032F4C) match Fighting Vipers' first 24 bytes: the same routines, changed. These are where the
fight rules, the training mode and the readable rewrites will start: Fighting Vipers'
`fv.h`, `round.cpp`, `fight.cpp`, `fightinput.cpp`, `training.cpp` are the models.

Work RAM the shared routines found from their own code: pad state 0x06094910 (FV 0x060943A0); the name
of the file being loaded 0x060948F0, its error byte 0x060948F4; the peripheral library's work area
0x060FFF80 (the same as Fighting Vipers').

## Game facts

- **Master/slave command queue** (the same scheme as Fighting Vipers): the master writes commands at
  [GBR+72] (GBR 0x060FFC00) and signals the slave by writing 0x21000000; the slave's loop is at 0x0605F464
  (table of handlers at 0x0605F4C0), it waits on its timer's input capture flag. The queue's start is at
  [0x0605DCE8]. The master resets the queue at 0x06060DEC.. and fills it straight after: sync point
  **06060DF0**. Without it: black screen after the title (the master waits for the slave at 0x0605B774
  forever).
- **Opening movie**: a second program at 0x060C0000-0x060E0000 (Duck TrueMotion, `MOV_DATA.DAT`, the
  movie starts at frame address 0x36A8). It has its own copies of the queue (reset at 0x060D6D74: sync
  point **060D6D78**), of the peripheral library (SendCommand 0x060D4510, pad data at [0x060D47F4]) and
  of a CD/file library that is a different build from the game's (it reads the volume descriptor, the
  directory and the movie through the drive; no lift yet). It hands the slave a job every vertical blank
  and needs the two CPUs side by side (`fine_slices = 060C0000-060E0000` in game.cfg; on the reference
  build `SATRECOMP_SHORT_SLICES=1`). Its main loop is at 0x060C211A; Start is tested at 0x060C0D08.
  The movie's clock (0x060E5AFC, 44,100 Hz) advances only when the sound driver's stream position (a
  word in sound RAM, from [0x25A00404] + 0xA0) moves, at 0x060C1180..0x060C1232.
- After the movie, or when it is skipped, the main program is started again (the SEGA logo).

## Open problems

1. **The opening movie freezes on its first frame** on the own machine. The movie's clock stops after
   35 steps because the sound driver's stream position stops moving. The own sound chip and 68000 are
   exact against Ymir's over this movie (`SATRECOMP_SOUND=check`, `SATRECOMP_M68K=check` on the reference
   build: 0 differences), and the reference build plays the movie with the same lifts and start-up
   (with `SATRECOMP_SHORT_SLICES=1`). So what differs is around the sound side on the own machine: how
   the stream's data reaches sound RAM (no SCU DMA goes there during the movie: the CPU or the SH-2's
   DMA does it), or the SMPC / sound timing. Next: find the writer of the stream buffer in sound RAM
   (`SATRECOMP_WRITERS`) and compare own and reference around it.
2. **Skipping the movie with Start** leaves the slave CPU running a stale queue entry (it jumps to
   address 0 at 0x0605F51E) when the game was started by `satrecomp/runtime/hle_bios.cpp`; with a BIOS
   image (`--bios`) the skip works and the game restarts at the SEGA logo. So a BIOS service or a piece
   of state the real BIOS leaves differs around the restart (the clock change, SMPC 0E/0F, re-starting
   the slave). Same on the reference build, so it is not the own machine.
3. The real BIOS runs 48,925 SCU DSP instructions (its own start-up); the game does not use the DSP.

## Debugging notes

- Explorer runs with `--auto SEED:900` press Start during the movie and so hit problem 2; use
  `movies/arcade_fight.txt` (Start taps from frame 400 to the mode select, then A).
- `SATRECOMP_CD_TRACE=1` lists the drive's commands on the own machine too.
- The reference build (`make setup SATRECOMP_REFERENCE=...`, `make ref`) with
  `--bios <image> --cpu interp` and `SATRECOMP_NO_LIFTS=1` is the original game, cycle exact.
