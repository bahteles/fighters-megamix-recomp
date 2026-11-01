# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Build environment for the Linux and Windows release builds (Ubuntu 24.04; x86_64, and ARM64 for the Linux
# ARM64 build, which has no Windows compiler):
#   docker build --platform linux/amd64 -t satrecomp-build tools/release
#   docker build --platform linux/arm64 -t satrecomp-build-arm64 tools/release
#   docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src satrecomp-build-arm64 tools/release/linux.sh
#   docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src satrecomp-build tools/release/linux.sh
#   docker run --rm --platform linux/amd64 -v "$PWD":/src -w /src satrecomp-build tools/release/windows.sh
FROM ubuntu:24.04
ARG TARGETARCH
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates g++ cmake ninja-build make python3 curl file zip unzip git zlib1g-dev \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols libdecor-0-dev libegl-dev libgl-dev libvulkan-dev \
    libpulse-dev libasound2-dev libudev-dev libdbus-1-dev libpipewire-0.3-dev \
    $([ "$TARGETARCH" = arm64 ] || echo g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix \
        binutils-mingw-w64-x86-64 libz-mingw-w64-dev) \
    && rm -rf /var/lib/apt/lists/*
# (Rust, for librashader: satrecomp/tools/librashader.sh; Ubuntu's rustup, which updates itself only through apt)
ENV RUSTUP_HOME=/opt/rustup CARGO_HOME=/opt/cargo PATH=/opt/cargo/bin:$PATH
RUN apt-get update && apt-get install -y --no-install-recommends rustup \
    && rm -rf /var/lib/apt/lists/* && rustup toolchain install 1.99.1 --profile minimal --no-self-update && rustup default 1.99.1 \
    && { [ "$TARGETARCH" = arm64 ] || rustup target add x86_64-pc-windows-gnu; }
