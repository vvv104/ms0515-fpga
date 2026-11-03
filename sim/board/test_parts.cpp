// The parts of the board the machine's software leaves alone, each worked
// by a test ROM (firmware/tests, tools/test_roms.py) run from reset in
// place of the machine's ROM, the bench on the connector it leads to.
#include <doctest/doctest.h>
#include <verilated.h>

#include "Vns4.h"
#include "cpu_writes.h"
#include "power.h"
#include "rom_socket.h"

#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kMasterMhz = 120;

std::vector<std::uint8_t> read_rom(const std::string& name)
{
    std::ifstream f(std::string(MS0515_FIRMWARE_DIR) + "/tests/" + name, std::ios::binary);
    REQUIRE(f);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

// The board from power-on with a test ROM in the sockets; `plug` puts what
// the bench has on the connectors before each tick.
class Bench {
public:
    explicit Bench(const std::string& rom) : m_(std::make_unique<Vns4>())
    {
        Verilated::randReset(0);
        disconnect(*m_);
        REQUIRE(plug_rom(*m_, read_rom(rom)));
    }

    void run_ms(std::uint64_t ms, const std::function<void(Vns4&)>& plug)
    {
        const std::uint64_t end = t_ + ms * 1000 * kMasterMhz;
        while (t_ < end) {
            power(*m_, t_++);
            plug(*m_);
            m_->clk = 0;
            m_->eval();
            m_->clk = 1;
            m_->eval();
            writes.see(*m_);
        }
    }

    CpuWrites writes;

private:
    std::unique_ptr<Vns4> m_;
    std::uint64_t t_ = 0;
};

}  // namespace

// XP5 through a loop-back plug: circuit 103 (the line out, D20.7) back into
// 104 (XP5.3, D48's receiver), 108 (D20.6, the C2 USART's DTR, to XP5.5
// through E2) into 107 (XP5.6), its CTS and DSR.  (The net named СТЫК С2
// 108 is another: D28.6 from the keyboard USART's DTR to XS2.5 through SA1.)
// The ROM starts at about 70 ms, after СТАРТ; the five bytes take some 4 ms.
TEST_CASE("С2: five bytes through a loop-back plug on XP5 come back as sent, DSR from DTR")
{
    Bench b("c2loop.rom");
    b.run_ms(80, [](Vns4& m) {
        m.x_XP5_p3 = m.n_STYK_S2_103;
        m.x_XP5_p3_oe = 1;
        m.x_XP5_p6 = m.nx_D20_6;
        m.x_XP5_p6_oe = 1;
    });
    REQUIRE(b.writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(b.writes.word_at(01006) == std::optional<std::uint16_t>(0));     // no timeouts
    const std::uint8_t sent[] = {0125, 0252, 0, 0377, 0123};
    for (unsigned i = 0; i < 5; ++i) {
        INFO("byte " << i);
        CHECK(b.writes.byte(01000 + i) == std::optional<std::uint8_t>(sent[i]));
    }
    CHECK((*b.writes.word_at(01010) & 0200) != 0);         // DSR
}
