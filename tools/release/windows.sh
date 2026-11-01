#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Release build for Windows x64, cross-compiled with MinGW-w64 on Linux (the container of
# tools/release/Dockerfile): build/release/<Game>-windows-x64.zip (launcher, game, SDL3.dll, licenses).
# The compiler's runtime is linked statically, so SDL3.dll is the only DLL beside the programs.
set -eu
. "$(dirname "$0")/common.sh"
BUILD="${BUILD:-$ROOT/build/release-windows}"
SDL_SHA256=c7ef65bd72eabac6e5b535411dbd8d5824d0aab24fd62ff8812666b336f18a9c
SDL="$DEPS/SDL3-$SDL_VERSION-mingw"

# SDL3's official MinGW development package
if [ ! -d "$SDL/x86_64-w64-mingw32" ]; then
    curl -fsSL -o "$DEPS/sdl3-mingw.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-devel-$SDL_VERSION-mingw.tar.gz"
    [ "$(sha256 "$DEPS/sdl3-mingw.tar.gz")" = "$SDL_SHA256" ] || { echo "error: SDL3 for MinGW: checksum mismatch" >&2; exit 1; }
    mkdir -p "$SDL" && tar -xzf "$DEPS/sdl3-mingw.tar.gz" -C "$SDL" --strip-components=1
fi

TOOLCHAIN="-DCMAKE_TOOLCHAIN_FILE=$ROOT/tools/release/mingw-w64.cmake"
PREFIX="-DCMAKE_PREFIX_PATH=$SDL/x86_64-w64-mingw32"
build_game "$BUILD/game" "$TOOLCHAIN" "$PREFIX" -DCMAKE_EXE_LINKER_FLAGS=-static -DZLIB_USE_STATIC_LIBS=ON
build_launcher "$BUILD/launcher" "$TOOLCHAIN" "$PREFIX" -DCMAKE_EXE_LINKER_FLAGS=-static

STAGE="$BUILD/stage/$PACKAGE"
rm -rf "$BUILD/stage" && mkdir -p "$STAGE"
cp "$BUILD/game/$GAME.exe" "$BUILD/launcher/$GAME-launcher.exe" "$SDL/x86_64-w64-mingw32/bin/SDL3.dll" "$STAGE/"
x86_64-w64-mingw32-strip "$STAGE"/*.exe
# (the Display page's shaders: librashader beside the game, opened when a shader is chosen, and the presets)
LIBRA=$(shaders "$STAGE" --windows)
if x86_64-w64-mingw32-objdump -p "$LIBRA/librashader.dll" | grep -i 'DLL Name' | grep -viE 'KERNEL32|ntdll|msvcrt|USER32|ADVAPI32|api-ms-|bcrypt|WS2_32|userenv|OPENGL32|GDI32|ole32|OLEAUT32|SHELL32'; then
    echo "error: librashader.dll needs a DLL that is not in the archive" >&2
    exit 1
fi
cp "$LIBRA/librashader.dll" "$STAGE/"
for dll in $(x86_64-w64-mingw32-objdump -p "$STAGE"/*.exe | awk '/DLL Name/ {print $3}' | sort -u); do
    case "$dll" in
    SDL3.dll | KERNEL32.dll | msvcrt.dll | USER32.dll | SHELL32.dll | ADVAPI32.dll | api-ms-win-crt-*.dll | ole32.dll | IMM32.dll | COMDLG32.dll | IPHLPAPI.DLL | WS2_32.dll) ;;
    *) echo "error: the programs need $dll, which is not in the archive" >&2; exit 1 ;;
    esac
done
cp "$ROOT/LICENSE" "$STAGE/LICENSE.txt"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/README.md" "$STAGE/"
licenses "$STAGE/licenses" "$BUILD/launcher" "$SDL/LICENSE.txt"
shader_licenses "$STAGE/licenses" "$LIBRA"
"$ROOT/tools/release/check_archive.sh" "$STAGE"
rm -f "$OUT/$PACKAGE-windows-x64.zip"
(cd "$BUILD/stage" && zip -qr "$OUT/$PACKAGE-windows-x64.zip" "$PACKAGE")
update_files windows-x64 "$STAGE" "$OUT/$PACKAGE-windows-x64.zip"
echo "wrote $OUT/$PACKAGE-windows-x64.zip"
