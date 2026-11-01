#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Starts what was built, with no game and no window: the game program lists its features, and the
# launcher draws its pages to pictures. Used by CI after `make` and `make launcher`.
set -eu
GAME="${GAME:-fighters_megamix}"
B="build/$GAME"
EXE=""
[ -f "$B/$GAME.exe" ] && EXE=.exe
features="$("$B/$GAME$EXE" --list-features | wc -l)"
[ "$features" -gt 0 ] || { echo "the game program lists no features"; exit 1; }
echo "ok: the game program starts ($features features)"
OUT="$B/smoke"
rm -rf "$OUT" && mkdir -p "$OUT/home"
HOME_VAR="$(echo "$GAME" | tr a-z A-Z)_HOME"
env SDL_VIDEO_DRIVER=dummy SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy "$HOME_VAR=$OUT/home" \
    "$B/$GAME-launcher$EXE" --screenshot "$OUT/page"
pages="$(ls "$OUT"/page*.png | wc -l)"
[ "$pages" -ge 3 ] || { echo "the launcher drew $pages pages, not 3"; exit 1; }
echo "ok: the launcher starts and draws its pages ($pages)"
