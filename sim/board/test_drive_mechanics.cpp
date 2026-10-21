// The МС 5305 drive's mechanics through its interface: the spindle and the
// index, READY on the third index, the head stepping on the trailing edge
// of ШАГ, track 0, the read pulses of a track, and writing.
#include <doctest/doctest.h>

#include "drive.h"

#include <cstdint>
#include <vector>

namespace {

using drive::Lines;
using drive::Mc5305;

constexpr std::uint64_t kUs = 120;                      // master ticks a microsecond
constexpr std::uint64_t kMs = 1000 * kUs;

std::vector<std::uint8_t> image_of(int sides)
{
    std::vector<std::uint8_t> img(static_cast<std::size_t>(sides) * drive::kTracks * drive::kSectors *
                                  drive::kSectorBytes);
    for (std::size_t i = 0; i < img.size(); ++i)
        img[i] = static_cast<std::uint8_t>(i * 13 + i / 512);
    return img;
}

// the board's lines idle: everything high (inactive), but this drive selected
Lines selected()
{
    Lines in;
    in.select0 = false;
    return in;
}

}  // namespace

TEST_CASE("МС 5305: with the motor on, an index a turn and READY on the third")
{
    Mc5305 d(image_of(2), drive::omega());
    Lines in = selected();
    in.motor = false;                                   // МВК L on
    std::vector<std::uint64_t> index_falls;
    std::uint64_t ready_at = 0;
    bool index_was = true;
    for (std::uint64_t t = 0; t < 700 * kMs; t += kUs / 4) {
        const drive::Outputs o = d.tick(t, in);
        if (!o.index && index_was) index_falls.push_back(t);
        index_was = o.index;
        if (!o.ready && ready_at == 0) ready_at = t;
    }
    REQUIRE(index_falls.size() >= 3);
    CHECK(index_falls[1] - index_falls[0] == 200 * kMs);
    CHECK(ready_at == index_falls[2]);                  // the third index
}

TEST_CASE("МС 5305: no motor, no index, not ready")
{
    Mc5305 d(image_of(2), drive::omega());
    const Lines in = selected();
    for (std::uint64_t t = 0; t < 500 * kMs; t += kMs) {
        const drive::Outputs o = d.tick(t, in);
        CHECK(o.index);
        CHECK(o.ready);
    }
}

TEST_CASE("МС 5305: the head steps on the trailing edge of ШАГ; track 0 at 0")
{
    Mc5305 d(image_of(2), drive::omega());
    Lines in = selected();
    std::uint64_t t = 0;
    auto step = [&](bool in_toward_centre) {
        in.direction = !in_toward_centre;               // НПШ high: out, toward track 0
        d.tick(t += kUs, in);
        in.step = false;
        const int before = d.head();
        d.tick(t += kUs, in);
        CHECK(d.head() == before);                      // no move on the leading edge
        in.step = true;
        d.tick(t += kUs, in);
        t += 3 * kMs;
    };
    CHECK(!d.tick(t, in).track0);                       // the head starts at 0
    step(true);
    step(true);
    CHECK(d.head() == 2);
    CHECK(d.tick(t, in).track0);                        // not at 0: high
    step(false);
    step(false);
    step(false);                                        // stops at 0
    CHECK(d.head() == 0);
    CHECK(!d.tick(t, in).track0);
}

TEST_CASE("МС 5305: the read pulses are the track's flux cells, 1 us low each")
{
    const std::vector<std::uint8_t> img = image_of(2);
    Mc5305 d(img, drive::omega());
    Lines in = selected();
    in.motor = false;
    const std::vector<std::uint8_t> cells = drive::track(drive::omega(), 0, 0, img.data() + 0);
    // the first 2000 cells after the index, sampled mid-pulse
    std::uint64_t t = 0;
    while (true) {
        const drive::Outputs o = d.tick(t, in);
        if (!o.index) break;
        t += kUs / 2;
    }
    const std::uint64_t turn_start = d.turn_start(t);
    for (std::size_t c = 0; c < 2000; ++c) {
        const std::uint64_t at = turn_start + c * 2 * kUs + kUs / 2;
        CHECK_MESSAGE(d.tick(at, in).data == (cells[c] == 0), "cell ", c);
    }
}
