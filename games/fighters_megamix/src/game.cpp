// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of Fighters Megamix Recomp: https://github.com/tmcleroy/fighters-megamix-recomp
// Fighters Megamix: the routines of the game that are replaced by C++ (see PROGRESS.md).
#include "game.h"
#include "gallery.h"
#include "geometry.h"
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

// Unlock everything (a cheat): what the game unlocks is in its save file FIGHTERS_MM (1,396 bytes, read to
// 0x0609FA00 and kept there), and the toolkit shows the game that file with these fields raised, keeping the
// player's own (Game::saveUnlocks). Found from the code that reads them:
// - 0x0609FAA8, a longword, one bit for each character or course unlocked: the screens that choose them take all
//   ones for all (character select, 0x0603DE30, offers 28 characters instead of 20, and 0x0603CB4C 42 places
//   instead of 35); a new file has 0x07FF85FF.
// - 0x0609FB02, a word of extras the game adds as it is played (the routine at 0x06037B58): 0x0080, 0x0600,
//   0x0100, 0x0040 and 0x0030, each when every course is cleared (0x0609FB04 = 0x01FF) and a count is reached.
// - 0x0609FB85 to 0x0609FB88, a byte each for the characters the game gives for time played and times started
//   (Mr. Meat and Palm Tree: 0x0609FB86 when it has been started 29 times, 0x0609FB87 when 0x0609FA80 passes
//   5,039) and the extra the same routine gives last.
void RegisterUnlocks() {
    Game &game = TheGame();
    game.features.push_back({"unlock_all", "Cheats", "Unlock everything",
                             "Every character, course and extra option, without playing for them. Your own save is kept as it is: "
                             "turned off, the game has what you have unlocked. From the next start or reset."});
    game.saveUnlocks = {"unlock_all", "FIGHTERS_MM", {
        {0xA8, 4, SaveField::kBits, 0xFFFFFFFF},
        {0x102, 2, SaveField::kBits, 0x07F0},
        {0x185, 1, SaveField::kBits, 1},
        {0x186, 1, SaveField::kBits, 1},
        {0x187, 1, SaveField::kBits, 1},
        {0x188, 1, SaveField::kBits, 1},
    }};
}

} // namespace

void RegisterGame() {
    const Lift audioWait = {"Sound mailbox timeout", 0x06014880, 0x1C, 0xAA8D47BFBDA96846ull, [](Guest &g) {
        // A00802FF can still be pending when the next sound command is queued after
        // returning to fighter select. If this wait expires, the game disables queued SFX
        // until it reinitializes sound. Give the sound CPU more time to clear the mailbox.
        constexpr uint32_t kTimeout = 0x060148A0;
        if (g.Read32(kTimeout) == 0x10000) {
            g.Write32(kTimeout, 0x40000);
        }
        return false; // Run the original bounded wait with the adjusted timeout.
    }};
    RegisterLifts(&audioWait, 1);
    RegisterShared();
    RegisterGallery();
    RegisterGeometry();
    RegisterUnlocks();
    TheGame().wideLetterboxLayer = 2; // NBG2 draws the credits replay bars.
    // Widescreen: the stages' panels (the cages, fences and walls) are listed by four of the engine's routines,
    // each with its own copy of a limit on how far off the picture's middle a panel may be (the toolkit's
    // shared::WidenAm2PanelLimits): two shaped as in Fighting Vipers (the limit loaded at 0x06020BC4 and
    // 0x06020CD0) and two more (0x06021F76, 0x060220E6). (The gauges stay in the middle: Fighting Vipers' way of
    // moving them out, a split of NBG2's rows, would split the title screen's text here too, which is on that
    // screen in the same rows; it needs the game's "a fight is on", not found yet.)
    TheGame().frame = [] { shared::WidenAm2PanelLimits({0x06020BC4, 0x06020CD0, 0x06021F76, 0x060220E6}); GalleryAfterFrame(); GeometryAfterFrame(); };
    TheGame().state = [](Archive &ar) { shared::State(ar); if(!ar.Saving())ResetGeometry(); };
}

} // namespace satrecomp
