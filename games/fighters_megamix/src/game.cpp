// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
// Fighters Megamix: the routines of the game that are replaced by C++ (see PROGRESS.md).
#include "game.h"
#include "lift.h"
#include "shared_lifts.h"
#include "state.h"

namespace satrecomp {

namespace {

// Routines Fighters Megamix shares byte for byte with Fighting Vipers (Sega's libraries, and AM2's engine):
// the same replacements, at this game's addresses (satrecomp/runtime/shared_lifts.h)
void RegisterShared() {
    using namespace shared;
    Register(kAm2LoadFile, 0x0602DE3C);
    Register(kAm2ReadPads, 0x0602F168);
    Register(kGfsOpen, 0x060483BC);
    Register(kGfsStart, 0x06048664);
    Register(kGfsPoll, 0x060489D8);
    Register(kGfsClose, 0x0604894C);
    Register(kGfsFread, 0x06049270);
    Register(kCdPlay, 0x06047F50);
    Register(kCdSeek, 0x06048014);
    Register(kCdGetCurStat, 0x060479C8);
    Register(kPerSendCommand, 0x0605C458);
    // The opening movie's player (a second program, loaded at 060C0000) has its own copy of the peripheral
    // library: the same routine, so Start skips the movie
    Register(kPerSendCommand, 0x060D4510, "SendCommand (movie player)");
}

} // namespace

void RegisterGame() {
    RegisterShared();
    TheGame().state = [](Archive &ar) { shared::State(ar); };
}

} // namespace satrecomp
