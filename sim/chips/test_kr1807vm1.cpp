// КР1807ВМ1 at its pins: power-up, each instruction's length against the
// T-11 Engineering Specification (App. B microcycles), interrupts.
#include <doctest/doctest.h>

#include "Vkr1807vm1.h"
#include "cpu_bench.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using Bench = CpuBench<Vkr1807vm1>;
constexpr int T = Bench::kTicksPerT;

// Registers set up for the cases: R0 = 2000, R1 = 2100, R2 = 2200, with
// pointers and a NOP wherever the cases look.
const std::vector<std::uint16_t> kSetup = {
    012700, 002000,     // MOV #2000, R0
    012701, 002100,     // MOV #2100, R1
    012702, 002200,     // MOV #2200, R2
};

void memory_for_cases(Bench& b)
{
    for (std::uint16_t a = 01000; a < 03000; a = static_cast<std::uint16_t>(a + 2))
        b.word(a) = 002400;                         // pointers into a quiet area
    b.word(001776) = 002400;
    for (std::uint16_t a = 002400; a < 002500; a = static_cast<std::uint16_t>(a + 2))
        b.word(a) = 000240;                         // NOPs
    b.word(000030) = 002400;                        // EMT vector
    b.word(000032) = 000340;
    b.word(000376) = 002400;                        // RTS / RTI pop this
    b.word(000400) = 000340;
}

// The T from the case's fetch to the next fetch.
int length_of(const std::vector<std::uint16_t>& instruction)
{
    Bench b;
    memory_for_cases(b);
    std::vector<std::uint16_t> program = kSetup;
    program.insert(program.end(), instruction.begin(), instruction.end());
    program.push_back(000240);
    b.load(0172000, program);
    b.power_up();
    b.run_fetches(5);
    const auto f = b.fetches();
    REQUIRE(f.size() >= 5);
    return static_cast<int>(f[4].tick - f[3].tick) / T;
}

struct Case {
    const char* name;
    std::vector<std::uint16_t> words;
    int microcycles;
    int extra_t = 0;                    // φD of an ASPI inside it
};

}  // namespace

TEST_CASE("КР1807ВМ1: power-up - -BCLR, the mode, the first fetch at the start address")
{
    Bench b;
    b.load(0172000, {000240, 000240});
    b.power_up();
    b.run_fetches(1);
    const auto f = b.fetches();
    REQUIRE(f.size() == 1);
    CHECK(f[0].address == 0172000);
    const double bclr_t = static_cast<double>(b.bclr_rose - b.pup_fell()) / T;
    CHECK(bclr_t >= 99.0);                          // tPBU 99T..100T
    CHECK(bclr_t <= 100.0);
    // the fetch begins a T before -RAS falls; tPFF 295T..315T
    const double fetch_t = static_cast<double>(f[0].tick - b.pup_fell()) / T - 1.0;
    CHECK(fetch_t >= 295.0);
    CHECK(fetch_t <= 315.0);
    // before it, no -RAS at all and one ASPI
    REQUIRE(b.log.size() == 2);
    CHECK(b.log[0].kind == Bench::Kind::Aspi);
}

TEST_CASE("КР1807ВМ1: the length of each instruction, App. B microcycles x 3T")
{
    const std::vector<Case> cases = {
        {"CLR R0", {005000}, 4},            {"CLR (R0)", {005010}, 7},
        {"CLR (R0)+", {005020}, 7},         {"CLR @(R0)+", {005030}, 9},
        {"CLR -(R0)", {005040}, 8},         {"CLR @-(R0)", {005050}, 10},
        {"CLR 10(R0)", {005060, 010}, 10},  {"CLR @10(R0)", {005070, 010}, 12},
        {"TST R0", {005700}, 4},            {"TST (R0)", {005710}, 6},
        {"TST @(R0)+", {005730}, 8},        {"TST -(R0)", {005740}, 7},
        {"TST @10(R0)", {005770, 010}, 11}, {"INCB (R0)+", {0105220}, 7},
        {"MOV R0,R1", {010001}, 4},         {"MOV (R0),(R1)", {011011}, 9},
        {"MOV #5,R1", {012701, 5}, 6},      {"MOV @10(R0),@20(R1)", {017071, 010, 020}, 19},
        {"MOV -(R0),@-(R1)", {014051}, 13}, {"MOVB (R0)+,R1", {0112001}, 6},
        {"CMP R0,R1", {020001}, 4},         {"CMP (R0),(R1)", {021011}, 8},
        {"CMP @#2000,10(R1)", {023761, 02000, 010}, 13},
        {"ADD -(R0),-(R1)", {064041}, 11},  {"BIT @(R0)+,R1", {033001}, 8},
        {"SUB R0,@(R1)+", {0160031}, 9},     {"BISB R0,(R1)", {0150011}, 7},
        {"XOR R0,R1", {074001}, 4},         {"XOR R0,(R1)", {074011}, 7},
        {"SWAB R1", {000301}, 4},           {"SXT R1", {006701}, 4},
        {"MFPS R1", {0106701}, 4},           {"MFPS (R1)", {0106711}, 7},
        {"MTPS R0", {0106400}, 8},           {"MTPS (R0)", {0106410}, 10},
        {"MTPS @10(R0)", {0106470, 010}, 15},
        {"JMP (R0)", {000110}, 5},          {"JMP (R0)+", {000120}, 6},
        {"JMP @(R0)+", {000130}, 6},        {"JMP -(R0)", {000140}, 6},
        {"JMP @-(R0)", {000150}, 7},        {"JMP 400(R0)", {000160, 0400}, 7},
        {"JMP @10(R0)", {000170, 010}, 9},
        {"JSR PC,(R0)", {004710}, 9},       {"JSR R5,@10(R0)", {004570, 010}, 13},
        {"RTS PC", {000207}, 7},            {"SOB R0,.", {077001}, 6},
        {"BR .+2", {000400}, 4},            {"BNE .+2", {001000}, 4},
        {"NOP", {000240}, 6},               {"SEC", {000261}, 6},
        {"MFPT", {000007}, 5},              {"EMT 0", {0104000}, 16},
        {"RTI", {000002}, 8},               {"RTT", {000006}, 8},
        {"HALT", {000000}, 14, 1},          {"RESET", {000005}, 39, 1},
    };
    for (const Case& c : cases) {
        CAPTURE(std::string(c.name));
        CHECK(length_of(c.words) == 3 * c.microcycles + c.extra_t);
    }
}

TEST_CASE("КР1807ВМ1: the T-11's own source values (UG §B.2)")
{
    Bench b;
    b.load(0172000, {012700, 002000,            // MOV #2000, R0
                     010020,                    // MOV R0, (R0)+: R0 + 2 is stored
                     012701, 002100,            // MOV #2100, R1
                     010141,                    // MOV R1, -(R1): R1 - 2 is stored
                     012702, 002200,            // MOV #2200, R2
                     010762, 000010,            // MOV PC, 10(R2) at 172020: 172024
                     000240});
    b.power_up();
    b.run_fetches(8);
    CHECK(b.word(002000) == 002002);
    CHECK(b.word(002076) == 002076);
    CHECK(b.word(002210) == 0172024);
}

TEST_CASE("КР1807ВМ1: a level-4 request is masked at priority 7, taken at 0 through 060")
{
    Bench b;
    memory_for_cases(b);
    b.word(000060) = 002400;
    b.word(000062) = 000340;
    b.load(0172000, {012706, 001000,            // MOV #1000, SP
                     000240,                    // NOP
                     0106427, 000000,            // MTPS #0
                     000240, 000240});
    b.ai = static_cast<std::uint8_t>(0xFF & ~(1u << 4) & ~(1u << 3));  // -CP0, -CP1 low: level 4, 060
    b.power_up();
    b.run_fetches(4);
    const auto f = b.fetches();
    REQUIRE(f.size() == 4);
    CHECK(f[1].address == 0172004);             // masked: the NOP runs
    CHECK(f[2].address == 0172006);             // MTPS #0
    CHECK(f[3].address == 002400);              // taken at its end: the handler
    // after MTPS's read of #0: IACK, PS and PC pushed below 1000, the vector
    std::vector<Bench::Access> after;
    for (const auto& a : b.log)
        if (a.tick > f[2].tick && a.tick < f[3].tick) after.push_back(a);
    REQUIRE(after.size() == 6);
    CHECK(after[0].kind == Bench::Kind::Read);  // the #0
    CHECK(after[1].kind == Bench::Kind::Iack);
    CHECK(after[2].kind == Bench::Kind::Write);
    CHECK(after[2].address == 000776);
    CHECK(after[2].data == 000000);             // the PS: priority 0
    CHECK(after[3].kind == Bench::Kind::Write);
    CHECK(after[3].address == 000774);
    CHECK(after[3].data == 0172012);            // the PC after MTPS
    CHECK(after[4].address == 000060);
    CHECK(after[5].address == 000062);
    // MTPS #0, then I N N W Ref N W R R N: 14 microcycles and IACK's φD
    CHECK(static_cast<int>(f[3].tick - f[2].tick) / T == 3 * 10 + 3 * 14 + 1);
}
