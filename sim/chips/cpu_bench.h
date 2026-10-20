// КР1807ВМ1 at its pins with a memory and the НС4 mode word around it: the
// bench of the processor's tests.  XTL1 is 7.5 MHz, 16 ticks of the
// 120 MHz master clock.
#pragma once

#include <verilated.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

template <class V>
class CpuBench {
public:
    static constexpr int kTicksPerT = 16;
    static constexpr std::uint16_t kMode = 0xF2FF;      // НС4: 16-bit static, start 172000

    // A transaction the bench saw: its kind, where, when -RAS (or -CAS
    // for an ASPI) fell.
    enum class Kind { Fetch, Read, Write, Aspi, Iack };
    struct Access {
        Kind kind;
        std::uint16_t address;
        std::uint16_t data;
        std::uint64_t tick;
    };

    CpuBench() : mem_(32768, 0) {}

    std::uint16_t& word(std::uint16_t address) { return mem_[address >> 1]; }
    void load(std::uint16_t address, const std::vector<std::uint16_t>& words)
    {
        for (std::uint16_t w : words) {
            word(address) = w;
            address = static_cast<std::uint16_t>(address + 2);
        }
    }

    // PUP high for a while, then low: the processor starts.
    void power_up()
    {
        pup_ = true;
        for (int i = 0; i < 40; ++i)
            tick();
        pup_ = false;
        pup_fell_ = t_;
    }

    // Runs until `n` more fetches have begun (or the tick budget is out).
    void run_fetches(int n, std::uint64_t budget = 200000)
    {
        const std::size_t want = fetch_count() + static_cast<std::size_t>(n);
        const std::uint64_t end = t_ + budget;
        while (fetch_count() < want && t_ < end)
            tick();
    }

    void run_ticks(std::uint64_t n)
    {
        for (std::uint64_t i = 0; i < n; ++i)
            tick();
    }

    std::size_t fetch_count() const
    {
        std::size_t n = 0;
        for (const Access& a : log)
            n += a.kind == Kind::Fetch ? 1u : 0u;
        return n;
    }

    std::vector<Access> fetches() const
    {
        std::vector<Access> f;
        for (const Access& a : log)
            if (a.kind == Kind::Fetch) f.push_back(a);
        return f;
    }

    V& m() { return *m_; }
    std::uint64_t now() const { return t_; }
    std::uint64_t pup_fell() const { return pup_fell_; }

    std::vector<Access> log;
    std::uint8_t ai = 0xFF;                 // AI7..AI0 pin levels
    bool ready = true;
    std::uint64_t bclr_rose = 0;

private:
    std::array<CData*, 16> dal_out()
    {
        V& m = *m_;
        return {&m.p17, &m.p16, &m.p15, &m.p14, &m.p13, &m.p12, &m.p11, &m.p10,
                &m.p9, &m.p7, &m.p6, &m.p5, &m.p4, &m.p3, &m.p2, &m.p1};
    }
    std::array<CData*, 16> dal_oe()
    {
        V& m = *m_;
        return {&m.p17_oe, &m.p16_oe, &m.p15_oe, &m.p14_oe, &m.p13_oe, &m.p12_oe, &m.p11_oe,
                &m.p10_oe, &m.p9_oe, &m.p7_oe, &m.p6_oe, &m.p5_oe, &m.p4_oe, &m.p3_oe,
                &m.p2_oe, &m.p1_oe};
    }
    std::array<CData*, 16> dal_in()
    {
        V& m = *m_;
        return {&m.p17_in, &m.p16_in, &m.p15_in, &m.p14_in, &m.p13_in, &m.p12_in, &m.p11_in,
                &m.p10_in, &m.p9_in, &m.p7_in, &m.p6_in, &m.p5_in, &m.p4_in, &m.p3_in,
                &m.p2_in, &m.p1_in};
    }

    // the levels the chip drives on DAL, and which lines it drives
    void chip_dal(std::uint16_t& value, std::uint16_t& driven)
    {
        value = driven = 0;
        const auto out = dal_out();
        const auto oe = dal_oe();
        for (int i = 0; i < 16; ++i) {
            value |= static_cast<std::uint16_t>((*out[static_cast<std::size_t>(i)] & 1u) << i);
            driven |= static_cast<std::uint16_t>((*oe[static_cast<std::size_t>(i)] & 1u) << i);
        }
    }

    // The memory and the board around the pins, from what the chip drives.
    void device()
    {
        V& m = *m_;
        std::uint16_t value, driven;
        chip_dal(value, driven);
        // while PUP is high the pins are being preset: nothing to see yet
        const bool ras = !pup_ && !m.p29, cas = !pup_ && !m.p30, sel0 = m.p25, sel1 = m.p24;
        const bool rw_low = m.p27_oe && (!m.p27 || !m.p28);
        std::uint16_t other = 0xFFFF;                  // pull-ups
        if (!m.p18)
            other = kMode;                             // -BCLR: the board's mode word
        if (ras && !ras_was_) {
            address_ = value;
            if (sel1 && !sel0)
                log.push_back({Kind::Iack, value, 0, t_});
            else if (!sel1)                            // (SEL 11 is a DMA grant)
                log.push_back({sel0 ? Kind::Fetch : (m.p27 && m.p28 ? Kind::Read : Kind::Write),
                               value, 0, t_});
        }
        if (cas && !cas_was_ && !ras)
            log.push_back({Kind::Aspi, 0, 0, t_});
        if (ras && cas && driven == 0 && m.p27 && m.p28 && !sel1)
            other = word(address_);
        if (ras && cas && rw_low) {
            std::uint16_t& w = word(address_);
            if (!m.p28) w = static_cast<std::uint16_t>((w & 0xFF00) | (value & 0x00FF));
            if (!m.p27) w = static_cast<std::uint16_t>((w & 0x00FF) | (value & 0xFF00));
            if (!log.empty() && log.back().kind == Kind::Write)
                log.back().data = w;
        }
        const std::uint16_t net = static_cast<std::uint16_t>((value & driven) | (other & ~driven));
        const auto in = dal_in();
        for (int i = 0; i < 16; ++i)
            *in[static_cast<std::size_t>(i)] = static_cast<CData>((net >> i) & 1u);
        m.p32 = ai & 1u; m.p33 = (ai >> 1) & 1u; m.p34 = (ai >> 2) & 1u; m.p35 = (ai >> 3) & 1u;
        m.p36 = (ai >> 4) & 1u; m.p37 = (ai >> 5) & 1u; m.p38 = (ai >> 6) & 1u; m.p39 = (ai >> 7) & 1u;
        m.p26 = ready;
        ras_was_ = ras;
        cas_was_ = cas;
    }

    void tick()
    {
        V& m = *m_;
        m.p22 = (t_ % kTicksPerT) < kTicksPerT / 2 ? 1 : 0;
        m.p23 = 0;
        m.p19 = pup_;
        device();
        const bool bclr_was = !m.p18;
        m.clk = 0;
        m.eval();
        m.clk = 1;
        m.eval();
        if (bclr_was && m.p18)
            bclr_rose = t_;
        ++t_;
    }

    std::unique_ptr<V> m_ = std::make_unique<V>();
    std::vector<std::uint16_t> mem_;
    std::uint64_t t_ = 0, pup_fell_ = 0;
    std::uint16_t address_ = 0;
    bool pup_ = true, ras_was_ = false, cas_was_ = false;
};
