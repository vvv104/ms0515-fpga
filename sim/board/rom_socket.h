// rom_socket: another ROM pair put in the sockets XS3/XS4 - an image of
// the 16-bit ROM (140000..177777 as little-endian words, 16 KB; the window
// at 160000 shows its upper 8 KB) written
// into the cells of D65 (the even bytes) and D77 (the odd bytes) of the
// board model (rtl/chips/k573rf4.sv).  The chips fill their cells from
// their CONTENTS at the model's first eval(), so the model is evaluated
// once before the image goes in.
#pragma once

#include "Vns4.h"
#include "Vns4___024root.h"
#include "rom_image.h"

// false, and nothing written, when the image is not a 16 KB ROM
inline bool plug_rom(Vns4& m, const std::vector<std::uint8_t>& image)
{
    if (image.size() != kRomBytes)
        return false;
    m.eval();
    fill_rom(m.rootp->ns4__DOT__D65__DOT__cells, m.rootp->ns4__DOT__D77__DOT__cells, image);
    return true;
}
