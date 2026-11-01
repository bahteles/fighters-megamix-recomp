# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
# Build a game with the satrecomp toolkit. Pick the game with GAME=<id>
# (a directory under games/ with a game.cfg); see satrecomp/build.mk for targets.
GAME ?= fighters_megamix
SR ?= satrecomp
include $(SR)/build.mk

# Nothing from the game, no emulator's sources, no personal paths (tools/audit.sh); run before every push
.PHONY: audit
audit:
	@tools/audit.sh
