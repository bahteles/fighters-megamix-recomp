# Releasing

Versions follow `MAJOR.MINOR.PATCH`, with `-beta.N` or `-rc.N` while a version is not final. A tag
is `v` and the version (`v0.1.0-beta.1`). A release with a `-` in its version is marked as a
pre-release on GitHub. Versions stay beta until every platform's download has been played by hand.

The downloads contain code translated from the game, so CI cannot build them: a maintainer with the
disc builds them and attaches them to the draft that the tag opens.

## Steps

1. **In a pull request:** set `VERSION`, and move what is under "Unreleased" in `CHANGELOG.md` to a
   new `## <version>` section. `make audit`, `make check` and `make check-rewrites` pass. Merge.
2. **Tag** the merged commit on `main` and push the tag:

   ```sh
   git switch main && git pull
   git tag -a "v$(cat VERSION)" -m "Fighters Megamix Recomp $(cat VERSION)"
   git push origin "v$(cat VERSION)"
   ```

   The `Release` workflow checks that the tag is `VERSION`, runs the audit, and opens a **draft**
   release whose notes are the changelog's section. It fails if the changelog has no such section.
3. **Build the downloads** from the tagged commit, on a Mac with Docker and your disc's block
   database (`work/fighters_megamix/blocks.txt`):

   ```sh
   git status --short            # must print nothing: build exactly what was tagged
   make gen
   tools/release/all.sh          # audit, then macOS, Linux x86_64 and ARM64, Windows, then SHA256SUMS.txt
   ```

   Each script refuses to pack an archive that holds game data, an emulator's name or a personal
   path.
4. **Try each download** before anyone else does: unpack it somewhere else, start the launcher, pick
   the disc image, play a round with a gamepad and with the keyboard. macOS on a Mac, the Linux one
   on a Steam Deck, the Windows one on a Windows PC.
5. **Upload and publish:**

   ```sh
   cd build/release
   gh release upload "v$(cat ../../VERSION)" FightersMegamix-macos.zip \
       FightersMegamix-linux-x86_64.tar.gz FightersMegamix-linux-arm64.tar.gz FightersMegamix-windows-x64.zip \
       SHA256SUMS.txt update-*.txt
   gh release edit "v$(cat ../../VERSION)" --draft=false
   ```

   The `update-<system>.txt` files are what the launcher updates itself from (its Game page's Updates
   card): each names its system's download and every file in it, with their SHA-256. A launcher offers
   the newest release that has a manifest for its system, a beta only to a beta. Upload them with every
   release, and nothing else beside the downloads and `SHA256SUMS.txt`. The launcher reads the releases
   through GitHub's public API, so it can find them only once this repository is public; until then its
   check says it couldn't reach them.

6. Add an empty "Unreleased" section to `CHANGELOG.md` for what comes next.

## If something is wrong after publishing

Don't replace the files of a published release. Fix it, raise the version (`-beta.N+1`, or the patch
number) and release again; say in the changelog what was wrong.

## Source for the binaries

The GPL-3.0 asks that the source of a binary be offered with it. The tag is that source: GitHub
attaches it to every release. Build only from a tagged, clean commit so that this stays true.
