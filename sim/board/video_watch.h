// video_watch: the video's word on its way from the DRAM to the shift
// registers, checked at the two edges that take it.  The latches D51, D57
// (К555ИР23) take Д00..15 H as ЧТ ВЗУ H rises; the shift registers load
// ДВ00..07 H (D50, D49) as ВК2 H falls and ДВ08..15 H (D56, D55) as ВК3 H
// falls, while ВК4 H (MODE) is high.  An edge is seen as the chips see it:
// the level after one tick and the one before, with the data of the later
// (rtl/chips/k555ir23.sv, k555ir16.sv).  What the latches take is checked
// against the cell the video read (DramWatch's shadow), what the registers
// load against what the latches took.
#pragma once

#include "Vns4.h"
#include "dram_watch.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>

class VideoWatch {
public:
    struct Stats {
        std::uint64_t latched = 0, latch_differ = 0, loads = 0, load_differ = 0;
        std::uint64_t unchecked = 0, lost = 0, stale = 0, out_of_order = 0;
        std::uint64_t requests = 0, requests_lost = 0;
    };

    void see(const Vns4& m, const DramWatch& dram, std::uint64_t t)
    {
        const std::uint16_t d = data(m), dv = video_data(m);
        const bool cht = m.n_CHT_VZU_H != 0, vk2 = m.n_VK2_H != 0, vk3 = m.n_VK3_H != 0;
        if (cht && !cht_was_)
            latch(dram, d, t);
        if (m.n_VK4_H && !vk2 && vk2_was_)
            load(dv & 0x00FF, 0x00FF, t);
        if (m.n_VK4_H && !vk3 && vk3_was_)
            load(dv & 0xFF00, 0xFF00, t);
        for (int i = kHistory - 1; i > 0; --i) {
            d_[i] = d_[i - 1];
            dv_[i] = dv_[i - 1];
        }
        d_[0] = d;
        dv_[0] = dv;
        trace_[t % kTrace] = {t, m.n_VAS_L != 0, m.n_VAK_L != 0, m.n_VB1_H != 0, m.n_VB0_H != 0, m.n_VTAKT_H != 0,
                              m.n_SBRAV_H != 0, address(m), dram_column(m),
                              m.n_TS15_L != 0, m.n_ARB_H != 0, m.nx_D43_3 != 0, m.nx_D47_9 != 0, m.nx_D47_8 != 0,
                              m.nx_D14_11 != 0, m.n_CHT_VZU_H != 0, m.n_RAZR_OZU_L != 0, m.n_VK2_H != 0,
                              m.n_VK3_H != 0, m.n_VK4_H != 0, m.n_STROB_H != 0, m.n_TRVZU_H != 0};
        request(m, t);
        cht_was_ = cht;
        vk2_was_ = vk2;
        vk3_was_ = vk3;
    }

    const Stats& stats() const { return stats_; }

    void report() const
    {
        std::printf("video words: %llu latched, %llu not as the cell; %llu loads, %llu not as latched\n",
                    static_cast<unsigned long long>(stats_.latched),
                    static_cast<unsigned long long>(stats_.latch_differ),
                    static_cast<unsigned long long>(stats_.loads),
                    static_cast<unsigned long long>(stats_.load_differ));
        std::printf("video words: %llu latched with no video read of a written cell, %llu never loaded, "
                    "%llu loads of a word loaded before\n",
                    static_cast<unsigned long long>(stats_.unchecked), static_cast<unsigned long long>(stats_.lost),
                    static_cast<unsigned long long>(stats_.stale));
        std::printf("video words: %llu read off another cell than the counters named\n",
                    static_cast<unsigned long long>(stats_.out_of_order));
        std::printf("video requests: %llu, %llu of them strobed while CHT VZU still held the request "
                    "flip-flop D98.1 reset (lost); the closest strobe after the reset ended: %lld ticks\n",
                    static_cast<unsigned long long>(stats_.requests),
                    static_cast<unsigned long long>(stats_.requests_lost), static_cast<long long>(closest_));
    }

private:
    static constexpr int kHistory = 6;
    static constexpr int kPrinted = 10;
    enum { kLost, kUnchecked, kLatch, kAddress, kStale, kLoad, kKinds };
    static constexpr std::uint64_t kTrace = 192;    // ticks kept for a trace
    static constexpr std::uint64_t kTraced = 3;     // address findings traced
    struct Line {
        std::uint64_t t;
        bool ras, cas, vb1, vb0, vtakt, sbrav;
        unsigned a, av_column;
        bool ts15, arb, d43_1qn, d47_2q, d47_2qn, d14, cht, razr, vk2, vk3, vk4, strob, trvzu;
    };

    void latch(const DramWatch& dram, std::uint16_t d, std::uint64_t t)
    {
        if (fresh_ != 0 && ++stats_.lost && shown(t, kLost))
            std::printf("%.6f s: CHT VZU latched %06o over %06o, its %s never loaded\n", seconds(t), d, latched_,
                        fresh_ == 3 ? "bytes" : fresh_ == 1 ? "low byte" : "high byte");
        latched_ = d;
        fresh_ = 3;
        sequence(dram, t);
        if (!dram.video() || dram.known(dram.addressed()) != 3) {
            // a cell not written yet; the processor's read only a finding
            if (++stats_.unchecked && !dram.video() && shown(t, kUnchecked))
                std::printf("%.6f s: CHT VZU latched %06o after the processor's read of cell %03zo:%03zo\n",
                            seconds(t), d, dram.addressed() >> 8, dram.addressed() & 0xFF);
            return;
        }
        ++stats_.latched;
        const std::uint16_t want = dram.word(dram.addressed());
        if (d != want && ++stats_.latch_differ && shown(t, kLatch)) {
            std::printf("%.6f s: CHT VZU latched %06o, cell %03zo:%03zo holds %06o; D before:", seconds(t), d,
                        dram.addressed() >> 8, dram.addressed() & 0xFF, want);
            for (const std::uint16_t v : d_)
                std::printf(" %06o", v);
            std::printf("\n");
        }
    }

    // The cell the DRAM took against the one the video address counters
    // named: another is a word off another address.
    void sequence(const DramWatch& dram, std::uint64_t t)
    {
        if (!dram.video())
            return;
        const std::size_t cell = dram.addressed(), named = dram.video_named();
        if (cell != named && ++stats_.out_of_order && shown(t, kAddress)) {
            std::printf("%.6f s: the video read cell %03zo:%03zo, the counters named %03zo:%03zo\n", seconds(t),
                        cell >> 8, cell & 0xFF, named >> 8, named & 0xFF);
            if (stats_.out_of_order <= kTraced)
                print_trace(t);
        }
    }

    // the strobes, the multiplexers' select, А0..7 and the counters'
    // column (АВ09..12, Р3, АВ13 as the multiplexers take them) tick by tick
    void print_trace(std::uint64_t t) const
    {
        for (std::uint64_t i = t + 1 - kTrace; i < t; ++i) {   // (this tick is not kept yet)
            const Line& l = trace_[i % kTrace];
            std::printf("  %llu: RAS %d CAS %d VB %d%d VTAKT %d SBRAV %d  A %03o  AV column %03o  TS15L %d ARB %d"
                        " D43.3 %d D47.9 %d D47.8 %d D14.11 %d CHT %d RAZR %d VK2 %d VK3 %d VK4 %d STROB %d TRVZU %d\n",
                        static_cast<unsigned long long>(l.t), l.ras, l.cas, l.vb1, l.vb0, l.vtakt, l.sbrav, l.a,
                        l.av_column, l.ts15, l.arb, l.d43_1qn, l.d47_2q, l.d47_2qn, l.d14, l.cht, l.razr, l.vk2,
                        l.vk3, l.vk4, l.strob, l.trvzu);
        }
    }

    // The video's request for its next word: СТРОБ H clocks D98.1 (ТРВЗУ H),
    // the word's ЧТ ВЗУ H resets it through D95 (СБРВЗУ L).  A strobe that
    // comes while the reset still holds is lost - the word is not read and
    // the registers load the one before again.  Kept: how many, and how
    // close the strobes came after a reset ended.
    void request(const Vns4& m, std::uint64_t t)
    {
        const bool strob = m.n_STROB_H != 0, reset = !m.n_SBRVZU_L;
        if (!reset && reset_was_)
            reset_end_ = t;
        if (strob && !strob_was_) {
            ++stats_.requests;
            if (reset)
                ++stats_.requests_lost;
            else if (reset_end_ != 0)
                closest_ = std::min(closest_, static_cast<std::int64_t>(t - reset_end_));
        }
        strob_was_ = strob;
        reset_was_ = reset;
    }

    static unsigned address(const Vns4& m)
    {
        const CData bits[8] = {m.n_A0_H, m.n_A1_H, m.n_A2_H, m.n_A3_H, m.n_A4_H, m.n_A5_H, m.n_A6_H, m.n_A7_H};
        unsigned v = 0;
        for (int i = 0; i < 8; ++i)
            v |= (bits[i] & 1u) << i;
        return v;
    }
    static unsigned dram_column(const Vns4& m)
    {
        const CData bits[8] = {m.n_AV09_H, m.n_AV10_H, m.n_AV11_H, m.n_AV12_H,
                               m.n_R3_H,   m.n_R3_H,   m.n_R3_H,   m.n_AV13_H};
        unsigned v = 0;
        for (int i = 0; i < 8; ++i)
            v |= (bits[i] & 1u) << i;
        return v;
    }

    void load(std::uint16_t got, std::uint16_t mask, std::uint64_t t)
    {
        ++stats_.loads;
        const unsigned byte = mask == 0x00FF ? 1u : 2u;
        if (!(fresh_ & byte) && ++stats_.stale && shown(t, kStale))
            std::printf("%.6f s: %s loaded %06o again, no CHT VZU since\n", seconds(t),
                        mask == 0x00FF ? "VK2" : "VK3", got);
        if (!(fresh_ & byte) && stale_traced_ < kTraced && ++stale_traced_)
            print_trace(t);
        fresh_ &= ~byte;
        if (got != (latched_ & mask) && ++stats_.load_differ && shown(t, kLoad)) {
            std::printf("%.6f s: %s loaded %06o, latched %06o; DV before:", seconds(t),
                        mask == 0x00FF ? "VK2" : "VK3", got, latched_ & mask);
            for (const std::uint16_t v : dv_)
                std::printf(" %06o", v & mask);
            std::printf("\n");
        }
    }

    static double seconds(std::uint64_t t) { return static_cast<double>(t) / 120e6; }

    // whether to print a finding: the first kPrinted of each kind each second
    bool shown(std::uint64_t t, int kind)
    {
        const std::uint64_t second = t / 120000000ull;
        if (second != second_) {
            second_ = second;
            printed_ = {};
        }
        return printed_[static_cast<std::size_t>(kind)]++ < kPrinted;
    }

    static std::uint16_t data(const Vns4& m)
    {
        const CData bits[16] = {m.n_D00_H, m.n_D01_H, m.n_D02_H, m.n_D03_H, m.n_D04_H, m.n_D05_H,
                                m.n_D06_H, m.n_D07_H, m.n_D08_H, m.n_D09_H, m.n_D10_H, m.n_D11_H,
                                m.n_D12_H, m.n_D13_H, m.n_D14_H, m.n_D15_H};
        return pack(bits);
    }
    static std::uint16_t video_data(const Vns4& m)
    {
        const CData bits[16] = {m.n_DV00_H, m.n_DV01_H, m.n_DV02_H, m.n_DV03_H, m.n_DV04_H, m.n_DV05_H,
                                m.n_DV06_H, m.n_DV07_H, m.n_DV08_H, m.n_DV09_H, m.n_DV10_H, m.n_DV11_H,
                                m.n_DV12_H, m.n_DV13_H, m.n_DV14_H, m.n_DV15_H};
        return pack(bits);
    }
    static std::uint16_t pack(const CData (&bits)[16])
    {
        std::uint16_t v = 0;
        for (int i = 0; i < 16; ++i)
            v = static_cast<std::uint16_t>(v | (bits[i] & 1u) << i);
        return v;
    }

    std::uint16_t d_[kHistory] = {}, dv_[kHistory] = {};
    std::uint16_t latched_ = 0;
    unsigned fresh_ = 0;                // the bytes of the latched word not loaded yet
    std::uint64_t second_ = 0;
    std::array<Line, kTrace> trace_ = {};
    std::array<int, kKinds> printed_ = {};
    std::uint64_t stale_traced_ = 0, reset_end_ = 0;
    std::int64_t closest_ = INT64_MAX;
    bool strob_was_ = false, reset_was_ = false;
    bool cht_was_ = false, vk2_was_ = false, vk3_was_ = false;
    Stats stats_;
};
