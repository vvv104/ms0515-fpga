// sound_log on made-up lines: the edges it keeps, the tones it finds in
// them, and the WAV it makes of the speaker's node.
#include <doctest/doctest.h>

#include "sound_log.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr std::uint64_t kSecond = SoundLog::kTicksPerSecond;

// a square wave of `hz` on the node from tick `from` for `ticks`, the node high before and after
void tone(SoundLog& log, std::uint64_t from, std::uint64_t ticks, std::uint64_t hz)
{
    const std::uint64_t half = kSecond / hz / 2;
    for (std::uint64_t t = from; t < from + ticks; ++t)
        log.see(t, ((t - from) / half) % 2 == 1);
    log.see(from + ticks, true);
}

std::uint32_t le32(const std::vector<std::uint8_t>& b, std::size_t at)
{
    return static_cast<std::uint32_t>(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | b[at + 3] << 24);
}

}  // namespace

TEST_CASE("sound_log: a still node has no edge, a square wave two a period")
{
    SoundLog log;
    for (std::uint64_t t = 0; t < 1000; ++t) log.see(t, true);
    CHECK(log.edges().empty());
    tone(log, 1000, kSecond / 100, 1000);               // 10 ms of 1 kHz: 10 periods
    CHECK(log.edges().size() == 20);
    CHECK(log.edges().front() == 1000);                 // the node falls as the tone begins
}

TEST_CASE("sound_log: the tones are the stretches of edges evenly apart")
{
    SoundLog log;
    log.see(0, true);
    tone(log, kSecond / 10, kSecond / 20, 1000);        // at 0.1 s, 50 ms of 1 kHz
    tone(log, kSecond / 2, kSecond / 10, 440);          // at 0.5 s, 100 ms of 440 Hz
    const auto tones = log.tones();
    REQUIRE(tones.size() == 2);
    CHECK(tones[0].from == kSecond / 10);
    CHECK(tones[0].hz == doctest::Approx(1000.0).epsilon(0.001));
    CHECK(static_cast<double>(tones[0].to - tones[0].from) / kSecond == doctest::Approx(0.05).epsilon(0.02));
    CHECK(tones[1].from == kSecond / 2);
    CHECK(tones[1].hz == doctest::Approx(440.0).epsilon(0.001));
}

TEST_CASE("sound_log: a single click is an edge and no tone")
{
    SoundLog log;
    log.see(0, true);
    log.see(500, false);
    log.see(501, false);
    CHECK(log.edges().size() == 1);
    CHECK(log.tones().empty());
}

TEST_CASE("sound_log: the WAV is 8-bit mono of the node, a sample the share of its time the node was high")
{
    SoundLog log;
    log.see(0, true);
    tone(log, kSecond / 10, kSecond / 10, 1000);        // 0.1 .. 0.2 s of 1 kHz
    const unsigned rate = 48000;
    const auto wav = log.wav(0, kSecond / 2, rate);     // half a second of it
    REQUIRE(wav.size() == 44 + rate / 2);
    CHECK(std::vector<std::uint8_t>(wav.begin(), wav.begin() + 4) == std::vector<std::uint8_t>{'R', 'I', 'F', 'F'});
    CHECK(le32(wav, 4) == wav.size() - 8);
    CHECK(le32(wav, 24) == rate);
    CHECK(le32(wav, 40) == rate / 2);
    const std::uint8_t high = wav[44];                  // the node at rest
    CHECK(wav[44 + rate / 20] == high);                 // 50 ms: still at rest
    std::size_t lows = 0, highs = 0;
    for (std::size_t i = 44 + rate / 10; i < 44 + rate / 5; ++i) {
        lows += wav[i] < 128;
        highs += wav[i] > 128;
    }
    CHECK(lows > rate / 25);                            // the tone: about half the samples each way
    CHECK(highs > rate / 25);
    CHECK(wav[44 + rate / 4] == high);                  // 250 ms: at rest again
}

TEST_CASE("sound_log: a WAV from a later tick begins with the node as it stood then")
{
    SoundLog log;
    log.see(0, true);
    log.see(100, false);                                // low from tick 100 on
    const auto wav = log.wav(kSecond / 10, kSecond / 5, 8000);
    REQUIRE(wav.size() == 44 + 800);
    CHECK(wav[44] < 128);
    CHECK(wav.back() < 128);
}
