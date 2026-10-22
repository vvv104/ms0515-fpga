// rom_socket: another ROM pair put in the sockets XS3/XS4 - an image of
// the 16-bit ROM (160000..177777 as little-endian words, 16 KB) written
// into the cells of D65 (the even bytes) and D77 (the odd bytes) of the
// board model (rtl/chips/k573rf4.sv).  The chips fill their cells from
// their CONTENTS at the model's first eval(), so the model is evaluated
// once before the image goes in.
#pragma once

#include "Vns4.h"
#include "Vns4___024root.h"

#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::size_t kRomBytes = 16384;

// false, and nothing written, when the image is not a 16 KB ROM
inline bool plug_rom(Vns4& m, const std::vector<std::uint8_t>& image)
{
    if (image.size() != kRomBytes)
        return false;
    m.eval();
    for (std::size_t i = 0; i < kRomBytes / 2; ++i) {
        m.rootp->ns4__DOT__D65__DOT__cells[i] = image[2 * i];
        m.rootp->ns4__DOT__D77__DOT__cells[i] = image[2 * i + 1];
    }
    return true;
}
