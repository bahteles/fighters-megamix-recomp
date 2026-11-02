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
    // Widescreen: the stages' panels (the cages, fences and walls) are listed by four of the engine's routines,
    // each with its own copy of a limit on how far off the picture's middle a panel may be (the toolkit's
    // shared::WidenAm2PanelLimits): two shaped as in Fighting Vipers (the limit loaded at 0x06020BC4 and
    // 0x06020CD0) and two more (0x06021F76, 0x060220E6). (The gauges stay in the middle: Fighting Vipers' way of
    // moving them out, a split of NBG2's rows, would split the title screen's text here too, which is on that
    // screen in the same rows; it needs the game's "a fight is on", not found yet.)
    TheGame().frame = [] { shared::WidenAm2PanelLimits({0x06020BC4, 0x06020CD0, 0x06021F76, 0x060220E6}); };
    TheGame().state = [](Archive &ar) { shared::State(ar); };
}

} // namespace satrecomp
