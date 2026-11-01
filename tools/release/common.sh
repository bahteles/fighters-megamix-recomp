# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Shared by the release scripts (sourced): where things are, and the two builds every platform makes.
# A release holds the game program, which contains code translated from the game (work/<id>/gen, made
# by `make gen` from the block database on the builder's machine), so only someone with the disc can
# make one. It holds no game data.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GAME="${GAME:-fighters_megamix}"
WORK="${WORK:-$ROOT/work/$GAME}"
OUT="$ROOT/build/release"
DEPS="$ROOT/build/deps"
export DEPS # (satrecomp/tools/librashader.sh makes the Display page's shaders there too)

# shaders FOLDER LIBRASHADER_ARGS...: librashader (built once, satrecomp/tools/librashader.sh) and the presets the
# Display page names, with the files they use, into FOLDER's shaders/; prints librashader's folder
shaders() {
    folder=$1
    shift
    libra=$("$ROOT/satrecomp/tools/librashader.sh" "$@")
    rm -rf "$folder/shaders" && cp -R "$libra/shaders" "$folder/shaders"
    echo "$libra"
}
# shader_licenses FOLDER LIBRASHADER: librashader's two licenses (it is used under the GPL-3.0)
shader_licenses() {
    cp "$2/librashader-LICENSE.md" "$1/librashader-MPL-2.0.txt"
    cp "$2/librashader-LICENSE-GPL.md" "$1/librashader-GPL-3.0.txt"
}
VERSION="${VERSION:-$(cat "$ROOT/VERSION")}"
SDL_VERSION=3.4.16
CFG="python3 $ROOT/satrecomp/tools/gamecfg.py"
NAME="$($CFG name "$GAME")"
# the archives' name: the game's with no spaces
PACKAGE="$(echo "$NAME" | tr -d ' ')"
mkdir -p "$OUT" "$DEPS"

if ! ls "$WORK"/gen/blocks_*.cpp > /dev/null 2>&1; then
    echo "error: no translated code in $WORK/gen: run 'make gen' with your block database first" >&2
    exit 1
fi

# build_game BUILD_DIR [cmake arguments]: the game on satrecomp's own machine (never the reference build)
build_game() {
    dir=$1
    shift
    cmake -S "$ROOT/satrecomp" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGAME="$GAME" -DWORK_DIR="$WORK" \
        -DSATRECOMP_REFERENCE=OFF "$@"
    cmake --build "$dir" --target "$GAME"
}

# build_launcher BUILD_DIR [cmake arguments]
build_launcher() {
    dir=$1
    shift
    cmake -S "$ROOT/satrecomp/launcher" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DGAME_NAME="$NAME" -DGAME_EXE="$GAME" -DDISC_NAME="$($CFG disc_name "$GAME")" \
        -DDISC_PRODUCT="$($CFG disc_product "$GAME")" -DDISC_VERSION="$($CFG disc_version "$GAME")" \
        -DAPP_ID="$($CFG app_id "$GAME")" -DHOME_ENV="$(echo "$GAME" | tr a-z A-Z)_HOME" \
        -DDISCLAIMER="$($CFG disclaimer "$GAME")" -DGAME_VERSION="$VERSION" -DUPDATE_REPO="$($CFG update_repo "$GAME")" "$@"
    cmake --build "$dir"
}

# licenses FOLDER LAUNCHER_BUILD_DIR SDL_LICENSE: the texts a binary must carry
licenses() {
    mkdir -p "$1"
    cp "$3" "$1/SDL3.txt"
    cp "$2/_deps/imgui-src/LICENSE.txt" "$1/DearImGui.txt"
    cp "$(find "$2/_deps/inter-src" -iname 'LICENSE*' | head -1)" "$1/Inter-OFL.txt"
}

# update_files PLATFORM TOP ARCHIVE: what the launcher updates itself from (satrecomp/recomp-common/launcher/
# src/update.h), $OUT/update-PLATFORM.txt: the archive ARCHIVE (its SHA-256 and size) and every file in TOP (the
# app, or the folder at the archive's top) with its SHA-256 and size. Uploaded beside the archives (docs/RELEASING.md).
update_files() {
    platform=$1 top=$2 archive=$3
    manifest="$OUT/update-$platform.txt"
    printf 'recomp-update 2\nversion %s\n' "$VERSION" > "$manifest"
    printf 'archive %s %s %s\n' "$(sha256 "$archive")" "$(wc -c < "$archive" | tr -d ' ')" "$(basename "$archive")" >> "$manifest"
    printf 'top %s\n' "$(basename "$top")" >> "$manifest"
    (cd "$top" && find . -type f | sed 's|^\./||' | LC_ALL=C sort) | while IFS= read -r path; do
        mode=-
        [ -x "$top/$path" ] && mode=x
        printf 'file %s %s %s %s\n' "$(sha256 "$top/$path")" "$(wc -c < "$top/$path" | tr -d ' ')" "$mode" "$path" >> "$manifest"
    done
    echo "wrote $manifest ($(grep -c '^file ' "$manifest") files)"
}

sha256() { if command -v sha256sum > /dev/null; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d" " -f1; }

# sdl_source: SDL3's source in $DEPS/SDL3-$SDL_VERSION (for the platforms that link it statically)
sdl_source() {
    if [ ! -d "$DEPS/SDL3-$SDL_VERSION" ]; then
        curl -fsSL -o "$DEPS/sdl3.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL3-$SDL_VERSION.tar.gz"
        [ "$(sha256 "$DEPS/sdl3.tar.gz")" = 7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68 ] \
            || { echo "error: SDL3 source: checksum mismatch" >&2; exit 1; }
        tar -xzf "$DEPS/sdl3.tar.gz" -C "$DEPS"
    fi
}

# sdl_static PREFIX [cmake arguments]: builds SDL3 as a static library into PREFIX, once
sdl_static() {
    prefix=$1
    shift
    if [ ! -f "$prefix/lib/cmake/SDL3/SDL3Config.cmake" ]; then
        sdl_source
        cmake -S "$DEPS/SDL3-$SDL_VERSION" -B "$prefix-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF \
            -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib "$@" > /dev/null
        cmake --build "$prefix-build" > /dev/null
        cmake --install "$prefix-build" > /dev/null
    fi
}
