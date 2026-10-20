// The emulator's core, linked as the reference model the board is compared
// against: it must build here and run the machine out of reset.
#include <doctest/doctest.h>

extern "C" {
#include <ms0515/core/board.h>
}

#include <memory>

TEST_CASE("reference model: the core comes out of reset at the ROM's start")
{
    auto board = std::make_unique<ms0515_board_t>();
    board_init(board.get());
    board_reset(board.get());
    // The T-11 starts at 172000 with priority 7 (the mode word of НС4).
    CHECK(board->cpu.r[7] == 0172000);
    CHECK(board->cpu.psw == 0340);
}
