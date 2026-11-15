// MiSTer's DDR3 as a core sees it (DDRAM_*, an Avalon port of 64-bit
// words): a request stands while BUSY is high and is taken at the first
// clock edge with BUSY low; reads come back in order, each a tick of
// DOUT_READY, some ticks later.  How many - and how often BUSY holds a
// request - is the test's to set: DDR3 behind the HPS bridge answers in
// some 100-200 ns, later while the scaler or the refresh has it.  Words
// never written read as 0, as the bench's chips power up under
// Verilated::randReset(0).
#pragma once

#include <cstdint>
#include <deque>
#include <random>
#include <unordered_map>

class Ddram {
public:
    // latency in master ticks, at least min_ticks, at most max_ticks; busy
    // one tick in busy_one_in, at random (0: never)
    Ddram(int min_ticks, int max_ticks, int busy_one_in, std::uint32_t seed = 1)
        : min_(min_ticks), max_(max_ticks), busy_one_in_(busy_one_in), rng_(seed)
    {
    }

    // Before the clock's falling half: BUSY and this tick's answer.
    template <class M>
    void present(M& m, std::uint64_t tick)
    {
        m.ddr_busy = busy_ ? 1 : 0;
        m.ddr_dout_ready = 0;
        if (!answers_.empty() && answers_.front().due <= tick) {
            m.ddr_dout = answers_.front().data;
            m.ddr_dout_ready = 1;
            answers_.pop_front();
        }
    }

    // After the falling half, before the rising edge: the request standing
    // now is taken at that edge unless BUSY holds it.
    template <class M>
    void take(const M& m, std::uint64_t tick)
    {
        if (!busy_) {
            const std::uint32_t addr = m.ddr_addr;
            if (m.ddr_we) {
                std::uint64_t& w = mem_[addr];
                for (int i = 0; i < 8; ++i)
                    if (m.ddr_be >> i & 1) {
                        const std::uint64_t lane = 0xffull << (8 * i);
                        w = (w & ~lane) | (m.ddr_din & lane);
                    }
                ++writes_;
            }
            if (m.ddr_rd) {
                std::uniform_int_distribution<int> lat(min_, max_);
                std::uint64_t due = tick + static_cast<std::uint64_t>(lat(rng_));
                if (!answers_.empty() && due <= answers_.back().due)
                    due = answers_.back().due + 1;      // in order, a tick each
                const auto it = mem_.find(addr);
                answers_.push_back({due, it == mem_.end() ? 0 : it->second});
                ++reads_;
            }
        }
        busy_ = busy_one_in_ > 0 && std::uniform_int_distribution<int>(0, busy_one_in_ - 1)(rng_) == 0;
    }

    std::uint64_t reads() const { return reads_; }
    std::uint64_t writes() const { return writes_; }

private:
    struct Answer {
        std::uint64_t due;
        std::uint64_t data;
    };
    int min_, max_, busy_one_in_;
    std::mt19937 rng_;
    bool busy_ = false;
    std::deque<Answer> answers_;
    std::unordered_map<std::uint32_t, std::uint64_t> mem_;
    std::uint64_t reads_ = 0, writes_ = 0;
};
