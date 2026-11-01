#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Checks a folder that is about to become a release archive: check_archive.sh FOLDER
# It must hold no game data (disc images, saves, states, the block database, generated sources), no
# emulator, and nothing that names the builder's home folder. Exits non-zero on a finding.
set -eu
DIR=$1
fail=0
bad() { echo "ARCHIVE CHECK FAIL: $1"; echo "$2" | head -10; fail=1; }

found="$(find "$DIR" -type f | grep -iE '\.(bin|cue|iso|chd|ccd|img|mdf|mds|state|sav|bkr)$|blocks.*\.txt$|blocks_[0-9]+\.cpp$|decode\.bin$' || true)"
[ -z "$found" ] || bad "game data or generated files" "$found"
found="$(find "$DIR" -type f -size +200000k || true)"
[ -z "$found" ] || bad "a file over 200 MB (a disc image?)" "$found"
for f in $(find "$DIR" -type f | tr ' ' '?'); do
    case "$(file -b "$f")" in
    *executable* | *"shared object"* | *"shared library"* | *Mach-O*)
        if strings -a "$f" | grep -qiE 'ymir|mednafen|yabause|kronos'; then bad "an emulator's name in a program" "$f"; fi
        paths="$(strings -a "$f" | grep -E '/Users/[A-Za-z]|/home/[a-z]|[A-Z]:\\Users\\' | head -3 || true)"
        [ -z "$paths" ] || bad "a home folder path in $f" "$paths"
        ;;
    esac
done
for name in LICENSE THIRD_PARTY_NOTICES.md; do
    [ -n "$(find "$DIR" -name "$name*" | head -1)" ] || bad "missing $name" "$DIR"
done
for name in SDL3.txt DearImGui.txt Inter-OFL.txt; do
    [ -n "$(find "$DIR" -name "$name" | head -1)" ] || bad "missing licenses/$name" "$DIR"
done
[ "$fail" = 0 ] || exit 1
echo "archive check passed: $DIR"
