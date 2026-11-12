// НС4 with the memory expansion module (plan 6a, rtl/board/ns4_ext.sv): the
// module's RAM disk worked by a test ROM (firmware/tests/exdisk.mac) the way
// EX.SYS works it - eight pages written through the data port, read back.
// What it settles: the module's netlist as the plant's tables give it, the
// four НС4 nets it takes on XS1 (netlist/ext/joins.csv) and НС4's green
// wire, and that the byte counter, stepped by reads alone, still gives
// writes their bytes in order (the processor reads before it writes).
#include <doctest/doctest.h>
#include <verilated.h>

#include "Vns4_ext.h"
#include "Vns4_ext___024root.h"
#include "cpu_writes.h"
#include "power.h"
#include "rom_image.h"

#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
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

// The board from power-on with exdisk.rom, run until the program has left
// its end mark (or 200 ms); `drawn` puts НС4's D80.5 back on Э3's net - the
// board without the green wire.
CpuWrites run_exdisk(bool drawn)
{
    Verilated::randReset(0);
    auto m = std::make_unique<Vns4_ext>();
    disconnect(*m);
    m->s_D80_5 = drawn ? 1 : 0;
    const std::vector<std::uint8_t> rom = read_rom("exdisk.rom");
    REQUIRE(rom.size() == kRomBytes);
    m->eval();
    fill_rom(m->rootp->ns4_ext__DOT__D65__DOT__cells, m->rootp->ns4_ext__DOT__D77__DOT__cells, rom);
    CpuWrites writes;
    const std::uint64_t end = 200ull * 1000 * kMasterMhz;
    for (std::uint64_t t = 0; t < end && !writes.word_at(01012); ++t) {
        power(*m, t);
        m->clk = 0;
        m->eval();
        m->clk = 1;
        m->eval();
        writes.see(*m);
    }
    return writes;
}

}  // namespace

TEST_CASE("RAM disk: eight pages through the data port come back as written, at every address bit")
{
    const CpuWrites writes = run_exdisk(false);
    REQUIRE(writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(writes.word_at(01010) == std::optional<std::uint16_t>(2048));
    CHECK(writes.word_at(01006) == std::optional<std::uint16_t>(0));
}

// Without the green wire НС4's own registers answer at the module's
// addresses too (D80 decodes АД05-07 alone): the two fight on the bus.
TEST_CASE("RAM disk: without НС4's green wire the pages do not come back")
{
    const CpuWrites writes = run_exdisk(true);
    REQUIRE(writes.word_at(01012) == std::optional<std::uint16_t>(052525));
    CHECK(writes.word_at(01006) != std::optional<std::uint16_t>(0));
}
