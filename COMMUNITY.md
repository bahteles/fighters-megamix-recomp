# Community build

This community fork is maintained by bahteles and is based on tmcleroy/fighters-megamix-recomp.
The original authors retain their credits. The GPL-3.0 license and third-party notices apply.

## Local modifications

- Additional sound mailbox wait time after returning to fighter selection.
- Widescreen corrections for victory sprites and credits replay letterboxing.
- Optional HD texture replacement from a ZIP selected in the launcher.
- F10 toggles the selected texture pack.
- Widescreen title and checkerboard menu backgrounds.
- Windows launcher starts without a console and launches games without a new console.

The texture ZIP is supplied separately by the user. This repository supplies source code only.
A legally obtained game disc image is required by the original build workflow.

## Development status

The Windows game and launcher were built and tested locally. Frame captures were used locally to
check selected rendering changes. No game captures or extracted resources are added by this fork.
The complete upstream determinism and rewrite checks have not yet been run for these modifications.
The toolkit and common launcher are locally modified copies. Upstream copy-integrity checks will
report these differences until the changes are maintained and synchronized through component forks.
This branch is a community development build and is not an upstream release.

The optional texture loader currently supports the local pack layout and includes game-specific
replacement paths. Generalizing that code is required before proposing the loader upstream.
