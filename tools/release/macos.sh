#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Release build for macOS, Apple silicon and Intel in one: "<Game>.app" (the launcher, with the game
# beside it) zipped as build/release/<Game>-macos.zip. SDL3 is linked statically and the programs run on
# macOS 13 and later. ARCHS=arm64 makes it for Apple silicon only (half the time). The app is signed ad hoc (not notarized), so macOS warns the first time it is opened.
set -eu
. "$(dirname "$0")/common.sh"
BUILD="${BUILD:-$ROOT/build/release-macos}"
SDL="$DEPS/sdl3-macos-universal"
ARCHS="${ARCHS:-arm64;x86_64}"
TARGET="-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DCMAKE_OSX_ARCHITECTURES=$ARCHS"

sdl_static "$SDL" $TARGET
# (the Homebrew SDL3, if there is one, must not be found instead)
ONLY="-DCMAKE_PREFIX_PATH=$SDL -DSDL3_DIR=$SDL/lib/cmake/SDL3"
build_game "$BUILD/game" $TARGET $ONLY
build_launcher "$BUILD/launcher" $TARGET $ONLY

APP="$BUILD/stage/$NAME.app"
rm -rf "$BUILD/stage" && mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
sed -e "s/@VERSION@/$VERSION/" -e "s/@NAME@/$NAME/" -e "s/@APP_ID@/$($CFG app_id "$GAME")/" -e "s/@EXE@/$GAME-launcher/" \
    "$ROOT/tools/release/Info.plist" > "$APP/Contents/Info.plist"
cp "$BUILD/game/$GAME" "$BUILD/launcher/$GAME-launcher" "$APP/Contents/MacOS/"
strip -x "$APP/Contents/MacOS/$GAME" "$APP/Contents/MacOS/$GAME-launcher"
# (the Display page's shaders: librashader in Frameworks, which the game opens when a shader is chosen, and the presets)
LIBRA=$(shaders "$APP/Contents/Resources")
mkdir -p "$APP/Contents/Frameworks"
cp "$LIBRA/librashader.dylib" "$APP/Contents/Frameworks/"
cp "$ROOT/LICENSE" "$ROOT/THIRD_PARTY_NOTICES.md" "$ROOT/README.md" "$APP/Contents/Resources/"
licenses "$APP/Contents/Resources/licenses" "$BUILD/launcher" "$DEPS/SDL3-$SDL_VERSION/LICENSE.txt"
shader_licenses "$APP/Contents/Resources/licenses" "$LIBRA"
for binary in "$APP/Contents/MacOS/"* "$APP/Contents/Frameworks/"*; do
    if otool -L "$binary" | grep -q '/opt/homebrew\|/usr/local'; then
        echo "error: $binary links a library outside macOS" >&2
        exit 1
    fi
done
for binary in "$APP/Contents/MacOS/"*; do
    for arch in $(echo "$ARCHS" | tr ';' ' '); do
        lipo "$binary" -verify_arch "$arch" || { echo "error: $binary has no $arch part" >&2; exit 1; }
    done
done
"$ROOT/tools/release/check_archive.sh" "$APP"
codesign --force --deep -s - "$APP"
rm -f "$OUT/$PACKAGE-macos.zip"
(cd "$BUILD/stage" && ditto -c -k --keepParent "$NAME.app" "$OUT/$PACKAGE-macos.zip")
echo "wrote $OUT/$PACKAGE-macos.zip"
update_files macos "$APP" "$OUT/$PACKAGE-macos.zip"
