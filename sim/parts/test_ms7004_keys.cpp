// ms7004_keys: a PC keyboard on the МС 7004's matrix (rtl/parts/
// ms7004_keys.sv; its tables from netlist/ms7004/pc_keys.csv).  A step
// here is 8 ticks (the core's is 20 ms).  The places: (ГР - 1) * 16 + (ВР - 1)
// of matrix.csv - Й/J SA12 49, Ю/@ SA58 103, Э/\ SA66 104, СУ SA6 80, ВР SA8
// 112, РУС/ЛАТ SA15 97.
#include <doctest/doctest.h>

#include "Vms7004_keys.h"

#include <memory>
#include <vector>

namespace {

constexpr int kSay = 49, kAt = 103, kBackslash = 104, kCtrl = 80, kShift = 112, kRusLat = 97;

class Keys {
public:
    explicit Keys(int mode, int rus)
    {
        m_->mode = static_cast<CData>(mode);
        m_->rus = static_cast<CData>(rus);
        m_->ps2_key = 0;
        run(4);
    }

    void key(int code, bool down)
    {
        toggle_ ^= 1;
        m_->ps2_key = static_cast<SData>(toggle_ << 10 | (down ? 1 : 0) << 9 | (code & 0x1FF));
        run(1);
    }

    void run(int ticks)
    {
        for (int i = 0; i < ticks; ++i) {
            m_->clk = 0;
            m_->eval();
            m_->clk = 1;
            m_->eval();
            for (int p = 0; p < 128; ++p) {
                const bool now = bit(p);
                if (now != was_[p]) changes.push_back(now ? p : -1 - p);
                was_[p] = now;
            }
        }
    }

    bool bit(int p) const { return (m_->pressed[p / 32] >> (p % 32)) & 1; }

    // the order the places went down (p) and up (-1 - p)
    std::vector<int> changes;

private:
    std::unique_ptr<Vms7004_keys> m_ = std::make_unique<Vms7004_keys>();
    int toggle_ = 0;
    bool was_[128] = {};
};

}  // namespace

TEST_CASE("ms7004_keys: by position - Q is Й, down while held")
{
    Keys k(1, 0);
    k.key(0x15, true);
    k.run(4);
    CHECK(k.bit(kSay));
    k.key(0x15, false);
    k.run(4);
    CHECK(!k.bit(kSay));
}

TEST_CASE("ms7004_keys: by character in ЛАТ - Shift+2 is @, Ю/@ with ВР let go, ВР back after")
{
    Keys k(0, 0);
    k.key(0x12, true);                  // the PC's Shift
    k.run(4);
    CHECK(k.bit(kShift));
    k.changes.clear();
    k.key(0x1E, true);                  // 2
    k.run(40);
    CHECK(k.bit(kAt));
    CHECK(!k.bit(kShift));
    k.key(0x1E, false);
    k.run(40);
    CHECK(!k.bit(kAt));
    CHECK(k.bit(kShift));
    CHECK(k.changes == std::vector<int>{-1 - kShift, kAt, -1 - kAt, kShift});
}

TEST_CASE("ms7004_keys: by character in РУС - \\ turns РУС/ЛАТ over for it, and back")
{
    Keys k(0, 1);
    k.key(0x5D, true);
    k.run(60);
    CHECK(k.bit(kBackslash));
    k.key(0x5D, false);
    k.run(60);
    CHECK(k.changes == std::vector<int>{kRusLat, -1 - kRusLat, kBackslash, -1 - kBackslash, kRusLat,
                                        -1 - kRusLat});
}

TEST_CASE("ms7004_keys: the modifiers by position in either mode - Ctrl is СУ")
{
    for (int mode : {0, 1}) {
        Keys k(mode, 0);
        k.key(0x14, true);
        k.run(4);
        CHECK(k.bit(kCtrl));
        k.key(0x14, false);
        k.run(4);
        CHECK(!k.bit(kCtrl));
    }
}
