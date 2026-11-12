// rom_image: a 16-bit ROM image (140000..177777 as little-endian words,
// 16 KB) split into the cells of the board's two К573РФ4 - D65 the even
// bytes, D77 the odd - whatever board model holds them (rom_socket.h for
// НС4's).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

constexpr std::size_t kRomBytes = 16384;

template <class Cells>
void fill_rom(Cells& even, Cells& odd, const std::vector<std::uint8_t>& image)
{
    for (std::size_t i = 0; i < kRomBytes / 2; ++i) {
        even[i] = image[2 * i];
        odd[i] = image[2 * i + 1];
    }
}
