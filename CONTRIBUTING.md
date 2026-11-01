# Contributing

Thanks for helping! Bug reports from real machines and controllers are the most valuable thing
during the beta, and code, docs and testing are all welcome.

## The rules that keep this project legal

These aren't negotiable; pull requests that break them are closed.

1. **Never add anything from the game.** No disc image or part of one, no extracted files,
   textures, models, sounds, text dumps, save files, save states, RAM dumps, video or screenshots
   (beyond the few maintained in `docs/screenshots/` for the README), and nothing generated from the
   game: the block database (`work/<id>/blocks.txt`), the generated C++ (`work/<id>/gen/`),
   disassembly listings. `.gitignore` covers `work/` and `build/`; keep it that way. The same goes
   for issues: attach logs, not game files.
2. **Never add a BIOS**, or point to where disc images, BIOS files or other copyrighted files can be
   found.
3. **Nothing from Sega's development kits or from leaked material**: no SDK headers, libraries,
   sample code or manuals, no leaked source, no names taken from a leaked symbol file. What you
   know about the game must come from your own disc and your own analysis of it.
4. **No emulator's code.** An emulator (Ymir) is the reference each part is checked against, and
   reading one to learn how the hardware behaves is how this is done; copying its code in is not.
   The reference build fetches it (`make setup`); neither it nor a patch to it is ever committed here.
5. **Only add code you have the right to contribute**, under the GPL-3.0. Code from other projects
   must have a compatible license and be credited in `THIRD_PARTY_NOTICES.md`.

Hand-written C++ that does what a routine of the game does (`games/fighters_megamix/src`) is fine
and welcome: write it yourself from your own analysis, the way the existing files do, and check it
against the game's code as [satrecomp/docs/READABLE.md](satrecomp/docs/READABLE.md) describes.

## Reporting bugs

Open an issue with:

- your system (OS and version, CPU, GPU) and how you installed (release build or source);
- your controller, and which features are on (Pure, Enhanced or your own choice);
- what happened and what you expected, and how to make it happen again;
- a save state made just before it helps us most, but **don't attach it** (it holds the game's
  memory): say that you have one, and describe the moment.

## Building and testing

[docs/BUILDING.md](docs/BUILDING.md) explains the build with your own disc image. Before any pull
request run `make audit`: it checks that no game files, emulator sources or personal paths are in
the repository. CI runs it too, and builds what can be built without the game on macOS, Linux and
Windows. Before a pull request that touches the translator, the machine or the game's code:

- `make check`: the interpreter and the translated code must give the same state after every
  frame, and two runs must be alike;
- `make check-rewrites`: every hand-written routine against the code it replaces;
- with every feature off the game must run exactly as before: the same movie must give the same
  hashes (`--movie`, `--hash-log`).

## Changing gameplay or controls

Anything that changes how the game plays, looks or sounds compared with the Saturn is an optional
**feature**, off in the Pure preset, with a switch in the launcher and the menu. With every feature
off the port must play exactly like the original. Nothing may add input lag or slow the game down:
measure when in doubt. Both the keyboard and gamepads must keep working.
See [satrecomp/docs/FEATURES.md](satrecomp/docs/FEATURES.md).

## Where things go

`satrecomp/` is the toolkit: nothing in it may be about one game. What is about Fighters Megamix
goes in `games/fighters_megamix/` (`game.cfg`, `src/`).

**The toolkit is a copy.** `satrecomp/` is made by the toolkit's `tools/sync-toolkit.sh` from the
toolkit's own repository, [satrecomp](https://github.com/tmcleroy/satrecomp), and `satrecomp/recomp-common/` by `tools/sync-common.sh` from
[recomp-common](https://github.com/tmcleroy/recomp-common); neither is ever changed here.
`make check-toolkit` and `make check-common` (both part of `make check` and `make check-rewrites`,
and run by CI) fail if a copy is not what was recorded. A change to the toolkit is made in its
repository, tried from here with `make SR=../satrecomp` (see [docs/BUILDING.md](docs/BUILDING.md)),
committed there, and brought here with `satrecomp/tools/sync-toolkit.sh`.

## Code style

- Match the surrounding code: naming, comment density and idiom. Comments explain why, and name
  the address of the routine a rewrite replaces.
- C++20; 4-space indents, braces on the same line, lines up to about 120 columns.
- Commit messages: a short summary line, then what changed and why.

## Code of conduct

Be kind and constructive. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
