# Community build

This community fork is maintained by bahteles and is based on tmcleroy/fighters-megamix-recomp.
The original authors retain their credits. The GPL-3.0 license and third-party notices apply.

## Fork highlights

- **Full 16:9 support** for fights, menus, victory screens and credits, plus aspect-aware Gallery presentation.
- **SFX bug fixes**, including sound synchronization when returning to fighter selection.
- **HD texture pack support** through an optional local ZIP selected in the launcher; packs are supplied separately.
- **In-game model Gallery** with rotation, zoom, color variants and armor options.

- **Frame rate selection**: Original / 30 / 60 / 120 FPS, with experimental polygon interpolation at 60/120 and the original simulation/audio clock. See [frame rate settings](docs/FRAMERATE.md).

- **Geometry correction**: optional higher-precision polygon positions to reduce wobble without changing game speed or audio.

## Local modifications

- Additional sound mailbox wait time after returning to fighter selection.
- Widescreen corrections for victory sprites and credits replay letterboxing.
- Optional HD texture replacement from a ZIP selected in the launcher.
- F10 toggles the selected texture pack, including while frozen with F4.
- Batched stage maps and indexed panel replacements, loaded per stage from the optional ZIP.
- Widescreen title and checkerboard menu backgrounds.
- Gallery entry in MODE SELECT, with model rotation, zoom, original color variants and armor states.
- Gallery presentation in the game window, using the selected CRT/display shader and optional HD textures.
- Aspect-aware placement of the Gallery entry in the HUD, with restoration of the game's aspect after closing Gallery.
- Optional external full-screen artwork for credits and records, streamed separately from fighter textures.
- Retain stage palette variants across Wolf's palette animation to avoid repeated atlas rebuilds.
- Windows launcher starts without a console and launches games without a new console.

The texture ZIP is supplied separately by the user. This repository supplies source code only.
Gallery models and interface artwork are also local resources, supplied separately; no extracted
models, menu artwork, fonts or texture packs are distributed here. See [Gallery setup](docs/GALLERY.md).
A legally obtained game disc image is required by the original build workflow.

## Development status

The Windows game and launcher were built and tested locally. Frame captures were used locally to
check selected rendering changes. A Gallery demonstration capture is maintained in `docs/screenshots/`
for the README. Extracted resources and texture packs are not added by this fork.
The complete upstream determinism and rewrite checks have not yet been run for these modifications.
The toolkit and common launcher are locally modified copies. Upstream copy-integrity checks will
report these differences until the changes are maintained and synchronized through component forks.
This branch is a community development build and is not an upstream release.

The optional texture loader currently supports the local pack layout and includes game-specific
replacement paths. Generalizing that code is required before proposing the loader upstream.
