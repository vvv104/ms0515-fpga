// ready_slips: the processor's waits for the memory as the board's bus
// shows them.  Two kinds.  A READY slip: the К1807ВМ1 gives a bus cycle one
// more microcycle (3T, 0.4 us) when READY is low as -RAS falls, and one
// more for every falling edge of READY while -RAS stays low
// (docs/t11-model.md, "READY"); on НС4 READY is ЗАДЕРЖ L from D98 and -RAS
// is ПВАС L.  A clock hold: the processor's clock ПТС H is D24.2's output,
// and D24.1 (clocked by ПВАС H, D = ВУ L, reset by the video's request
// ТРВЗУ H) holds it high through D23.4 until the video has had the memory
// - the phase lasts as long as the hold, not the clock's 8 ticks.  The
// cycle's kind is on РЕЖ1, РЕЖ0 (SEL1, SEL0) as ПВАС L falls - SEL0 alone
// a fetch, neither a data read or write, SEL1 alone an interrupt
// acknowledge.  The counts are what the pace of a corner is made of
// (docs/baseline.md, open list 10).
#pragma once

#include <cstdint>
#include <cstdio>

class ReadySlips {
public:
    enum Kind { kFetch, kData, kIack, kOther, kKinds };
    struct Counts {
        std::uint64_t cycles[kKinds] = {}, slips[kKinds] = {};
        std::uint64_t holds = 0, held = 0;      // clock holds, and the ticks they held in all
    };
    static constexpr std::uint64_t kPhase = 8;      // ticks of a phase of the 7.5 MHz clock at 120 MHz
    static constexpr std::uint64_t kHold = 12;      // a phase this long is a hold

    // the bus's lines, every tick
    void see(bool ras_low, bool ready_low, bool sel0, bool sel1)
    {
        if (ras_low && !ras_was_) {
            kind_ = sel0 ? (sel1 ? kOther : kFetch) : (sel1 ? kIack : kData);
            ++counts_.cycles[kind_];
            if (ready_low) slip();
        } else if (ras_low && ready_low && !ready_was_) {
            slip();
        }
        ras_was_ = ras_low;
        ready_was_ = ready_low;
    }

    // the processor's clock, every tick
    void see_clock(bool pts)
    {
        if (pts == pts_was_) {
            ++phase_;
            return;
        }
        if (phase_ >= kHold) {
            ++counts_.holds;
            counts_.held += phase_ - kPhase;
        }
        phase_ = 1;
        pts_was_ = pts;
    }

    const Counts& counts() const { return counts_; }
    std::uint64_t total() const { return total_; }
    std::uint64_t held() const { return counts_.held; }

    void report() const
    {
        static const char* const names[kKinds] = {"fetches", "data", "IACK", "other"};
        std::printf("READY slips: %llu in %llu bus cycles -", static_cast<unsigned long long>(total_),
                    static_cast<unsigned long long>(counts_.cycles[kFetch] + counts_.cycles[kData] +
                                                    counts_.cycles[kIack] + counts_.cycles[kOther]));
        for (int k = 0; k < kKinds; ++k)
            std::printf("%s %s %llu of %llu", k ? "," : "", names[k],
                        static_cast<unsigned long long>(counts_.slips[k]),
                        static_cast<unsigned long long>(counts_.cycles[k]));
        std::printf("\nclock holds: %llu, %llu ticks held in all (%.1f ticks a hold)\n",
                    static_cast<unsigned long long>(counts_.holds), static_cast<unsigned long long>(counts_.held),
                    counts_.holds ? static_cast<double>(counts_.held) / static_cast<double>(counts_.holds) : 0.0);
    }

private:
    void slip()
    {
        ++counts_.slips[kind_];
        ++total_;
    }

    Counts counts_;
    std::uint64_t total_ = 0, phase_ = 0;
    Kind kind_ = kOther;
    bool ras_was_ = false, ready_was_ = false, pts_was_ = false;
};
