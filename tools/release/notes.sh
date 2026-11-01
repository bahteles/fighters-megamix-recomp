#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Prints the section of CHANGELOG.md for a version: notes.sh 0.1.0-beta.1
# Fails if there is none, so that a tag cannot be released without its notes.
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
awk -v v="$1" '
    /^## / { if (found) exit; found = ($2 == v); next }
    found { print }
' "$ROOT/CHANGELOG.md" > "${TMPDIR:-/tmp}/notes.$$"
if ! grep -q '[^[:space:]]' "${TMPDIR:-/tmp}/notes.$$"; then
    rm -f "${TMPDIR:-/tmp}/notes.$$"
    echo "error: CHANGELOG.md has no section '## $1'" >&2
    exit 1
fi
cat "${TMPDIR:-/tmp}/notes.$$"
rm -f "${TMPDIR:-/tmp}/notes.$$"
