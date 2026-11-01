#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# All release builds on an Apple silicon Mac with Docker (e.g. OrbStack): macOS natively, Linux x86_64 and
# Windows in the x86_64 build image, Linux ARM64 in the ARM64 one (tools/release/Dockerfile). Writes
# build/release/ with the four downloads, SHA256SUMS.txt and each system's update-<system>.txt (what the
# launcher updates itself from). Needs the translated code (`make gen`), so it is run by someone with
# the disc. See docs/RELEASING.md.
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
tools/audit.sh
docker build --platform linux/amd64 -q -t satrecomp-build tools/release > /dev/null
docker build --platform linux/arm64 -q -t satrecomp-build-arm64 tools/release > /dev/null
rm -f build/release/update-*.txt # (written again by each script)
tools/release/macos.sh
docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src satrecomp-build tools/release/linux.sh
docker run --rm --platform linux/arm64 -v "$ROOT":/src -w /src satrecomp-build-arm64 tools/release/linux.sh
docker run --rm --platform linux/amd64 -v "$ROOT":/src -w /src satrecomp-build tools/release/windows.sh
cd build/release
shasum -a 256 *.zip *.tar.gz > SHA256SUMS.txt
cat SHA256SUMS.txt
# (each manifest names its system's archive as it is)
for manifest in update-*.txt; do
    set -- $(sed -n 's/^archive //p' "$manifest")
    if [ "$(shasum -a 256 "$3" | cut -d' ' -f1)" != "$1" ]; then
        echo "error: $manifest does not name $3 as it is" >&2
        exit 1
    fi
done
echo "update manifests: $(ls update-*.txt | tr '\n' ' ')"
echo "version $(cat "$ROOT/VERSION"): release files in $ROOT/build/release"
