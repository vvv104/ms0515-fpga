// How fast the board model runs: 5 ms of the machine, in wall time.
#include <verilated.h>

#include "Vns4.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>

int main()
{
    auto m = std::make_unique<Vns4>();
    m->x_BQ1_p1_oe = 1;
    m->x_BQ2_p1_oe = 1;
    const std::uint64_t ticks = 5000ull * 120ull;
    const auto start = std::chrono::steady_clock::now();
    for (std::uint64_t t = 0; t < ticks; ++t) {
        m->x_BQ1_p1 = (t % 15) < 8 ? 1 : 0;
        m->x_BQ2_p1 = (t % 8) < 4 ? 1 : 0;
        m->clk = 0;
        m->eval();
        m->clk = 1;
        m->eval();
    }
    const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("%.2f s for 5 ms of the machine: %.0f ticks/s, %.0fx slower\n", s,
                static_cast<double>(ticks) / s, s / 0.005);
    return 0;
}
