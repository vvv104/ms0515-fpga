// The МС 7004 keyboard generated from its netlist (rtl/board/ms7004.sv):
// its processor running its own firmware, against its ТО and the firmware's
// listing (Alex's).
// - Power-on (the listing, 133..146): the four indicators of port 2 (ОЖИД,
//   КМП, ФКС, СТОП КАДР) lit while it beeps twice, about 2 kHz (ТО 5.4:
//   "звонка частотой порядка 2 кГц"), then every one dark (Table 3's
//   footnote: "панель индикации погашена").  ЛАТ, on P15, the beep's own
//   OUTL P1 sets from RAM not yet cleared (21H): on the board as the RAM
//   came up, here off (the model's RAM starts at zero).
// - Fig. 3: the scan's running one, 13 us on each ВР line.
// - A key (Table 1): А/A, SA34 on ВР5/ГР5, sends 322.
// - Table 3: the computer's 253 (AB, ROM-B's question) answered 001 000.
// The bench holds RESET low for the supply's first millisecond (C3) and
// plays the matrix board: a pressed key puts its ВР line's pulse on its ГР.
// The lines are as the board's XS2 has them - idle low, a start bit high, a
// data bit high for a 0 - at 4800 baud.  An indicator is lit while its pad
// is low (D3 pulls its LED's cathode).
#include <doctest/doctest.h>

#include "Vms7004.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <utility>
#include <vector>

namespace {

constexpr std::uint64_t kUs = 120;
constexpr std::uint64_t kBit = 1000000 * kUs / 4800;

class Keyboard {
public:
    std::set<std::pair<int, int>> pressed;     // (ВР, ГР)
    std::uint64_t t = 0;
    std::vector<std::uint8_t> sent;
    std::vector<std::vector<std::uint64_t>> pulses = std::vector<std::vector<std::uint64_t>>(17);
    std::uint64_t bell_edges = 0;

    Keyboard()
    {
#define MS7004_INPUT(name) m_.name = 0;
#include "../../rtl/board/ms7004_inputs.inc"
#undef MS7004_INPUT
    }

    void run(std::uint64_t ticks)
    {
        for (std::uint64_t i = 0; i < ticks; ++i) tick();
    }

    // a byte from the computer on Вход, from now
    void send(std::uint8_t b)
    {
        std::uint64_t at = t + kBit;
        line_.push_back({at, true});
        for (int i = 0; i < 8; ++i)
            line_.push_back({at + static_cast<std::uint64_t>(i + 1) * kBit, ((b >> i) & 1) == 0});
        line_.push_back({at + 9 * kBit, false});
    }

    // the indicators of port 2, and all five: lit (pad low) or dark
    int lit_p2() const { return !m_.n_OZHID + !m_.n_KMP + !m_.n_FKS + !m_.n_STOP_KADR; }
    int lit() const { return lit_p2() + !m_.n_LAT; }

private:
    int vr(int k) const
    {
        switch (k) {
        case 1: return m_.n_VR1; case 2: return m_.n_VR2; case 3: return m_.n_VR3; case 4: return m_.n_VR4;
        case 5: return m_.n_VR5; case 6: return m_.n_VR6; case 7: return m_.n_VR7; case 8: return m_.n_VR8;
        case 9: return m_.n_VR9; case 10: return m_.n_VR10; case 11: return m_.n_VR11; case 12: return m_.n_VR12;
        case 13: return m_.n_VR13; case 14: return m_.n_VR14; case 15: return m_.n_VR15; default: return m_.n_VR16;
        }
    }

    // the pads: ГР1..ГР8 are XS24, XS25, XS26, XS27, XS28, XS29, XS31, XS30
    void matrix()
    {
        int gr[9] = {};
        for (const auto& [k, g] : pressed)
            gr[g] |= vr(k);
        m_.x_XS_p24 = static_cast<CData>(gr[1]); m_.x_XS_p25 = static_cast<CData>(gr[2]);
        m_.x_XS_p26 = static_cast<CData>(gr[3]); m_.x_XS_p27 = static_cast<CData>(gr[4]);
        m_.x_XS_p28 = static_cast<CData>(gr[5]); m_.x_XS_p29 = static_cast<CData>(gr[6]);
        m_.x_XS_p31 = static_cast<CData>(gr[7]); m_.x_XS_p30 = static_cast<CData>(gr[8]);
        m_.x_XS_p24_oe = m_.x_XS_p25_oe = m_.x_XS_p26_oe = m_.x_XS_p27_oe = 1;
        m_.x_XS_p28_oe = m_.x_XS_p29_oe = m_.x_XS_p30_oe = m_.x_XS_p31_oe = 1;
    }

    void talk()
    {
        while (!line_.empty() && line_.front().first <= t) {
            level_ = line_.front().second;
            line_.pop_front();
        }
        m_.x_XT1_p3 = level_ ? 1 : 0;
        m_.x_XT1_p3_oe = 1;
    }

    void listen()
    {
        const bool line = m_.n_VYHOD != 0;
        if (!in_frame_ && line && !was_) {
            in_frame_ = true;
            start_ = t;
        }
        if (in_frame_ && t == start_ + kBit / 2 + 9 * kBit) {
            sent.push_back(byte_);
            in_frame_ = false;
            byte_ = 0;
        } else if (in_frame_) {
            for (int b = 0; b < 8; ++b)
                if (t == start_ + kBit / 2 + static_cast<std::uint64_t>(b + 1) * kBit && !line)
                    byte_ = static_cast<std::uint8_t>(byte_ | 1 << b);
        }
        was_ = line;
    }

    void watch()
    {
        for (int k = 1; k <= 16; ++k) {
            const int v = vr(k);
            if (v && !vr_was_[k]) rise_[k] = t;
            if (!v && vr_was_[k]) pulses[static_cast<std::size_t>(k)].push_back(t - rise_[k]);
            vr_was_[k] = v;
        }
        const bool bell = m_.nx_D2_6 != 0;
        if (bell != bell_was_) ++bell_edges;
        bell_was_ = bell;
    }

    void tick()
    {
        m_.x_C3_p1 = 0;
        m_.x_C3_p1_oe = t < 1000 * kUs ? 1 : 0;         // the supply rising: RESET low
        matrix();
        talk();
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        listen();
        watch();
        ++t;
    }

    std::unique_ptr<Vms7004> model_ = std::make_unique<Vms7004>();
    Vms7004& m_ = *model_;
    bool in_frame_ = false, was_ = false, level_ = false, bell_was_ = false;
    std::uint64_t start_ = 0;
    std::uint8_t byte_ = 0;
    int vr_was_[17] = {};
    std::uint64_t rise_[17] = {};
    std::deque<std::pair<std::uint64_t, bool>> line_;
};

}  // namespace

TEST_CASE("МС 7004: at power-on port 2's indicators lit while it beeps at about 2 kHz, then all dark")
{
    auto kb = std::make_unique<Keyboard>();
    kb->run(50000 * kUs);
    CHECK(kb->lit_p2() == 4);
    const std::uint64_t edges = kb->bell_edges;
    kb->run(10000 * kUs);
    const double khz = static_cast<double>(kb->bell_edges - edges) / 2.0 / 10.0;
    MESSAGE("the bell at " << khz << " kHz");
    CHECK(khz > 1.5);
    CHECK(khz < 3.5);
    kb->run(140000 * kUs);
    CHECK(kb->lit() == 0);
}

TEST_CASE("МС 7004: the scan - each ВР line a pulse of about 13 us (Fig. 3)")
{
    auto kb = std::make_unique<Keyboard>();
    kb->run(200000 * kUs);
    for (int k = 1; k <= 16; ++k) {
        INFO("ВР" << k);
        const auto& p = kb->pulses[static_cast<std::size_t>(k)];
        REQUIRE(p.size() > 5);
        CHECK(p.back() >= 8 * kUs);
        CHECK(p.back() <= 18 * kUs);
    }
}

TEST_CASE("МС 7004: А/A pressed (SA34, ВР5/ГР5) sends 322 (Table 1)")
{
    auto kb = std::make_unique<Keyboard>();
    kb->run(200000 * kUs);
    const std::size_t before = kb->sent.size();
    kb->pressed.insert({5, 5});
    kb->run(40000 * kUs);
    REQUIRE(kb->sent.size() > before);
    CHECK(kb->sent[before] == 0322);
}

TEST_CASE("МС 7004: the computer's 253 (AB, ROM-B's question) is answered 001 000 (Table 3)")
{
    auto kb = std::make_unique<Keyboard>();
    kb->run(200000 * kUs);
    const std::size_t before = kb->sent.size();
    kb->send(0253);
    kb->run(40000 * kUs);
    REQUIRE(kb->sent.size() >= before + 2);
    CHECK(kb->sent[before] == 001);
    CHECK(kb->sent[before + 1] == 000);
}
