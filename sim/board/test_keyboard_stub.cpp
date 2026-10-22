// The keyboard stub on XS2: asked for its ID (AB), it answers 01 00 as the
// МС 7004 (an LK201) does, on the line's own levels and speed.
#include <doctest/doctest.h>

#include "keyboard_stub.h"

#include <cstdint>
#include <vector>

namespace {

constexpr std::uint64_t kBit = keyboard::Stub::kBitTicks;

// the line's level a bit at a time for one 8N1 frame: idle 0, the start
// bit 1, a data bit 1 for a 0 (the line's sense, ВЫХКЛ L / ВХКЛ L)
std::vector<bool> frame(std::uint8_t b)
{
    std::vector<bool> levels = {true};
    for (int i = 0; i < 8; ++i)
        levels.push_back(((b >> i) & 1) == 0);
    levels.push_back(false);                        // stop
    return levels;
}

// the bytes on a line sampled tick by tick (a bit's middle)
std::vector<std::uint8_t> decode(const std::vector<bool>& line, std::uint64_t step)
{
    std::vector<std::uint8_t> out;
    for (std::size_t t = 0; t + 1 < line.size(); ++t) {
        if (!line[t] && line[t + 1]) {                              // a start bit begins
            const std::size_t start = t + 1;
            std::uint8_t b = 0;
            for (int i = 0; i < 8; ++i) {
                const std::size_t mid = start + static_cast<std::size_t>((static_cast<std::uint64_t>(i + 1) * kBit + kBit / 2) / step);
                if (mid < line.size() && !line[mid]) b = static_cast<std::uint8_t>(b | 1 << i);
            }
            out.push_back(b);
            t = start + static_cast<std::size_t>(10 * kBit / step);
        }
    }
    return out;
}

}  // namespace

TEST_CASE("keyboard stub: AB is answered with 01 00")
{
    keyboard::Stub s;
    const std::uint64_t step = kBit / 16;           // sixteen looks a bit
    std::vector<bool> answer;
    std::uint64_t t = 0;
    for (int i = 0; i < 50; ++i, t += step) answer.push_back(s.tick(t, false));
    for (const bool level : frame(0xAB))
        for (int k = 0; k < 16; ++k, t += step) answer.push_back(s.tick(t, level));
    for (int i = 0; i < 16 * 40; ++i, t += step) answer.push_back(s.tick(t, false));
    CHECK(decode(answer, step) == std::vector<std::uint8_t>{0x01, 0x00});
    CHECK(s.heard() == std::vector<std::uint8_t>{0xAB});
}

TEST_CASE("keyboard stub: other bytes are heard and not answered")
{
    keyboard::Stub s;
    const std::uint64_t step = kBit / 16;
    std::vector<bool> answer;
    std::uint64_t t = 0;
    for (const std::uint8_t b : {std::uint8_t{0x11}, std::uint8_t{0x90}})
        for (const bool level : frame(b))
            for (int k = 0; k < 16; ++k, t += step) answer.push_back(s.tick(t, level));
    for (int i = 0; i < 16 * 30; ++i, t += step) answer.push_back(s.tick(t, false));
    CHECK(decode(answer, step).empty());
    CHECK(s.heard() == std::vector<std::uint8_t>{0x11, 0x90});
}

TEST_CASE("keyboard stub: keys pressed go out one after another, after an answer in progress")
{
    keyboard::Stub s;
    const std::uint64_t step = kBit / 16;
    std::vector<bool> line;
    std::uint64_t t = 0;
    for (const bool level : frame(0xAB))
        for (int k = 0; k < 16; ++k, t += step) line.push_back(s.tick(t, level));
    s.press(t, {0257, 0316});                       // СУ and S while 01 00 still waits to go
    s.press(t + 1, {0263});
    for (int i = 0; i < 16 * 80; ++i, t += step) line.push_back(s.tick(t, false));
    CHECK(decode(line, step) == std::vector<std::uint8_t>{0x01, 0x00, 0257, 0316, 0263});
}
