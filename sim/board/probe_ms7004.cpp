// A probe of the generated МС 7004 (not a test): where its processor goes
// from power-on, the opcodes it takes, for finding what the tests miss.
#include "Vms7004.h"
#include "Vms7004___024root.h"

#include <cstdint>
#include <cstdio>
#include <memory>

int main(int argc, char** argv)
{
    const std::uint64_t ms = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 20;
    auto model = std::make_unique<Vms7004>();
    Vms7004& m = *model;
#define MS7004_INPUT(name) m.name = 0;
#include "../../rtl/board/ms7004_inputs.inc"
#undef MS7004_INPUT
    auto* r = m.rootp;
    int last_pc = -1, shown = 0;
    static bool seen[4096] = {};
    for (std::uint64_t t = 0; t < ms * 120000; ++t) {
        m.x_C3_p1 = 0;
        m.x_C3_p1_oe = t < 120000 ? 1 : 0;
        m.clk = 0;
        m.eval();
        m.clk = 1;
        m.eval();
        const int pc = r->ms7004__DOT__D1__DOT__pc;
        if (pc != last_pc && !seen[pc & 4095] && shown < 400) {
            seen[pc & 4095] = true;
            std::printf("%8.3f ms pc %03X ir %02X a %02X phase %d\n", static_cast<double>(t) / 120000.0, pc,
                        r->ms7004__DOT__D1__DOT__ir, r->ms7004__DOT__D1__DOT__a, r->ms7004__DOT__D1__DOT__phase);
            ++shown;
        }
        last_pc = pc;
    }
    return 0;
}
