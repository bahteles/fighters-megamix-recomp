# Changelog

## Unreleased

- Unlock everything (a cheat on the Features page): every character, Mr. Meat and Palm Tree among them, every
  course and the extra options, without playing for them. The game is shown its save with them unlocked; the
  save kept is your own, so with it off the game has what you have unlocked. From the next start or reset.
  Toolkit at satrecomp a7b9c65 (save unlocks).
- Sound effects in fights: they stopped after "Round 1, Fight!", a hiss in their place, while the music went
  on. The file library's Fread is the game's own again, reading through the CD drive (the toolkit's
  replacement read at once, and the game loads its sound through it); loading before a fight takes about two
  seconds longer.
- Widescreen: the stages' cages and fences reach the picture's edges (they ended a little past the 4:3
  picture's sides). The gauges stay in the middle for now.
- README: screenshots (a fight in 16:9 and in 4:3, the character select, the launcher).
- README: a short section per feature, with the picture and the launcher in docs/PICTURE.md and docs/LAUNCHER.md.

## 0.1.0-beta.2

- The opening movie plays (it froze on its first frames), and skipping it with Start works (it crashed);
  the attract loop goes round. Toolkit at satrecomp's fix for both.

## 0.1.0-beta.1

- First release: Fighters Megamix boots, its menus work and arcade fights play on satrecomp's own machine,
  with Fighting Vipers' file, CD music and controller routines (shared in the toolkit). The opening
  movie freezes on its first frame (games/fighters_megamix/PROGRESS.md).
