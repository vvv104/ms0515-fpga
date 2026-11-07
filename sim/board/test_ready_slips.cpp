// ready_slips on made-up lines: a slip for READY low as -RAS falls, one
// for each falling edge of READY while -RAS stays low, none otherwise;
// the cycles and the slips told by the processor's SEL lines.
#include <doctest/doctest.h>

#include "ready_slips.h"

#include <cstdint>
#include <initializer_list>

namespace {

// one bus cycle: -RAS low for `ticks`, READY as `ready_low` says for each tick
void cycle(ReadySlips& slips, bool sel0, bool sel1, const std::initializer_list<bool>& ready_low)
{
    for (bool low : ready_low) slips.see(true, low, sel0, sel1);
    slips.see(false, false, sel0, sel1);
}

}  // namespace

TEST_CASE("ready_slips: READY high through a cycle slips nothing")
{
    ReadySlips slips;
    cycle(slips, true, false, {false, false, false, false});
    CHECK(slips.counts().cycles[ReadySlips::kFetch] == 1);
    CHECK(slips.total() == 0);
}

TEST_CASE("ready_slips: READY low as -RAS falls is one slip, and stays one while it stays low")
{
    ReadySlips slips;
    cycle(slips, true, false, {true, true, true, true});
    CHECK(slips.counts().cycles[ReadySlips::kFetch] == 1);
    CHECK(slips.counts().slips[ReadySlips::kFetch] == 1);
    CHECK(slips.total() == 1);
}

TEST_CASE("ready_slips: each falling edge of READY while -RAS is low is a slip")
{
    ReadySlips slips;
    cycle(slips, false, false, {false, true, true, false, true});       // two falls after -RAS
    CHECK(slips.counts().cycles[ReadySlips::kData] == 1);
    CHECK(slips.counts().slips[ReadySlips::kData] == 2);
    cycle(slips, false, false, {true, false, true});                    // low at the fall, and once more
    CHECK(slips.counts().slips[ReadySlips::kData] == 4);
}

TEST_CASE("ready_slips: READY falling between cycles is nobody's slip")
{
    ReadySlips slips;
    slips.see(false, false, false, false);
    slips.see(false, true, false, false);
    slips.see(false, false, false, false);
    cycle(slips, true, false, {false, false});
    CHECK(slips.total() == 0);
}

TEST_CASE("ready_slips: the kinds by SEL1, SEL0 as -RAS falls")
{
    ReadySlips slips;
    cycle(slips, true, false, {true});          // SEL0 alone: a fetch
    cycle(slips, false, false, {true});         // neither: data
    cycle(slips, false, true, {true});          // SEL1 alone: an interrupt acknowledge
    cycle(slips, true, true, {true});           // both
    CHECK(slips.counts().cycles[ReadySlips::kFetch] == 1);
    CHECK(slips.counts().cycles[ReadySlips::kData] == 1);
    CHECK(slips.counts().cycles[ReadySlips::kIack] == 1);
    CHECK(slips.counts().cycles[ReadySlips::kOther] == 1);
    CHECK(slips.total() == 4);
}

TEST_CASE("ready_slips: the clock's own phases are no hold, a long one is held by its excess")
{
    ReadySlips slips;
    for (int i = 0; i < 10; ++i)                                     // five periods of 8 + 8
        for (int k = 0; k < 8; ++k) slips.see_clock(i % 2 == 0);
    slips.see_clock(true);
    CHECK(slips.counts().holds == 0);
    CHECK(slips.held() == 0);
    for (int k = 0; k < 31; ++k) slips.see_clock(true);              // a high phase of 32 ticks
    slips.see_clock(false);
    CHECK(slips.counts().holds == 1);
    CHECK(slips.held() == 24);
    for (int k = 0; k < 9; ++k) slips.see_clock(false);              // 10 low: the clock's own jitter
    slips.see_clock(true);
    CHECK(slips.counts().holds == 1);
}
