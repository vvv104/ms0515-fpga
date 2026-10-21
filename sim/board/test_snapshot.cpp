// A snapshot of the bench's parts: the drive and the keyboard stub saved in
// the middle of their work and put back into new ones, which then go on
// exactly as the first ones do.
#include <doctest/doctest.h>

#include "drive.h"
#include "keyboard_stub.h"
#include "snapshot.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

// a stream in memory with the Verilator serialize operators' shape
struct Memory {
    std::vector<std::uint8_t> bytes;
    std::size_t at = 0;

    template <class T>
    Memory& operator<<(const T& v)
    {
        const auto* p = reinterpret_cast<const std::uint8_t*>(&v);
        bytes.insert(bytes.end(), p, p + sizeof v);
        return *this;
    }
    template <class T>
    Memory& operator>>(T& v)
    {
        std::memcpy(&v, bytes.data() + at, sizeof v);
        at += sizeof v;
        return *this;
    }
};

std::vector<std::uint8_t> image()
{
    std::vector<std::uint8_t> img(2u * drive::kTracks * drive::kSectors * drive::kSectorBytes);
    for (std::size_t i = 0; i < img.size(); ++i) img[i] = static_cast<std::uint8_t>(i * 31 + 7);
    return img;
}

// the board's lines at tick t: the drive selected, its motor on, a step now and then
drive::Lines lines(std::uint64_t t)
{
    drive::Lines l;
    l.select0 = false;
    l.motor = false;
    l.direction = false;
    l.step = (t / 120'000) % 7 != 3;
    return l;
}

bool same(const drive::Outputs& a, const drive::Outputs& b)
{
    return a.index == b.index && a.track0 == b.track0 && a.protect == b.protect && a.data == b.data &&
           a.ready == b.ready;
}

}  // namespace

TEST_CASE("snapshot: the drive goes on from a snapshot as it would have")
{
    drive::Mc5305 first(image(), drive::omega());
    std::uint64_t t = 0;
    for (; t < 30'000'000; t += 60) first.tick(t, lines(t));
    Memory m;
    SaveArchive<Memory> save{m};
    first.state(save);
    drive::Mc5305 second(std::vector<std::uint8_t>(16), drive::omega());   // a different disk at first
    LoadArchive<Memory> load{m};
    second.state(load);
    CHECK(second.head() == first.head());
    for (std::uint64_t k = 0; k < 2'000'000; ++k, t += 60)
        REQUIRE(same(second.tick(t, lines(t)), first.tick(t, lines(t))));
}

TEST_CASE("snapshot: the keyboard stub goes on mid-answer")
{
    keyboard::Stub first;
    std::uint64_t t = 0;
    const std::uint64_t bit = keyboard::Stub::kBitTicks;
    // AB on the line, then the answer under way
    std::vector<bool> frame = {true};
    for (int i = 0; i < 8; ++i) frame.push_back(((0xAB >> i) & 1) == 0);
    frame.push_back(false);
    for (const bool level : frame)
        for (std::uint64_t k = 0; k < bit; k += 100, t += 100) first.tick(t, level);
    for (int k = 0; k < 2000; ++k, t += 100) first.tick(t, false);
    Memory m;
    SaveArchive<Memory> save{m};
    first.state(save);
    keyboard::Stub second;
    LoadArchive<Memory> load{m};
    second.state(load);
    CHECK(second.heard() == first.heard());
    for (int k = 0; k < 40'000; ++k, t += 100)
        REQUIRE(second.tick(t, false) == first.tick(t, false));
}
