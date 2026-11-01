#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Release build for Linux x86_64 (and SteamOS: Steam Deck, Steam Machine) or ARM64 (aarch64: ARM handhelds,
# single-board computers), for the processor it runs on: build/release/<Game>-linux-x86_64.tar.gz or
# <Game>-linux-arm64.tar.gz (launcher, game, licenses), and its update-linux-*.txt. Run on Linux, e.g. in the
# container of tools/release/Dockerfile (--platform linux/amd64 or linux/arm64). SDL3 and the C++ library are linked statically, so the
# programs need only the system's usual X11/Wayland, audio and Vulkan libraries.
set -eu
. "$(dirname "$0")/common.sh"
case "$(uname -m)" in
    x86_64) SYSTEM=linux-x86_64 ;;
    aarch64 | arm64) SYSTEM=linux-arm64 ;;
    *) echo "error: no Linux release for $(uname -m)" >&2; exit 1 ;;
esac
# (x86_64's folders keep their old names; ARM64's are its own)
SUFFIX=$([ "$SYSTEM" = linux-x86_64 ] || echo -arm64)
BUILD="${BUILD:-$ROOT/build/release-linux$SUFFIX}"
SDL="$DEPS/sdl3-linux$SUFFIX"

sdl_static "$SDL"
STATIC="-static-libstdc++ -static-libgcc"
build_game "$BUILD/game" -DCMAKE_PREFIX_PATH="$SDL" -DCMAKE_EXE_LINKER_FLAGS="$STATIC"
build_launcher "$BUILD/launcher" -DCMAKE_PREFIX_PATH="$SDL" -DCMAKE_EXE_LINKER_FLAGS="$STATIC"

STAGE="$BUILD/stage/$PACKAGE"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
cp "$BUILD/game/$GAME" "$BUILD/launcher/$GAME-launcher" "$STAGE/"
strip "$STAGE/$GAME" "$STAGE/$GAME-launcher"
# (the Display page's shaders: librashader beside the game, opened when a shader is chosen, and the presets)
LIBRA=$(shaders "$STAGE")
cp "$LIBRA/librashader.so" "$STAGE/"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/README.md" "$STAGE/"
licenses "$STAGE/licenses" "$BUILD/launcher" "$DEPS/SDL3-$SDL_VERSION/LICENSE.txt"
shader_licenses "$STAGE/licenses" "$LIBRA"
"$ROOT/tools/release/check_archive.sh" "$STAGE"
rm -f "$OUT/$PACKAGE-$SYSTEM.tar.gz"
tar -C "$BUILD/stage" -czf "$OUT/$PACKAGE-$SYSTEM.tar.gz" "$PACKAGE"
echo "wrote $OUT/$PACKAGE-$SYSTEM.tar.gz"
update_files "$SYSTEM" "$STAGE" "$OUT/$PACKAGE-$SYSTEM.tar.gz"
