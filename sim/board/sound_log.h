// sound_log: what a run hears - the speaker's node (D93's open collectors,
// through R61 to XP7.2) tick by tick.  It keeps the ticks at which the node
// changed; of them it makes the tones (stretches of edges evenly apart: the
// timer's square wave) and a WAV file to listen to - 8-bit mono, a sample
// the share of its time the node was high, so the picture is the node's and
// nothing of a loudspeaker's.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

class SoundLog {
public:
    static constexpr std::uint64_t kTicksPerSecond = 120'000'000;
    static constexpr std::size_t kToneEdges = 4;        // fewer edges evenly apart are clicks, not a tone

    struct Tone {
        std::uint64_t from, to;                         // its first and last edge
        double hz;
    };

    // the node at master tick t
    void see(std::uint64_t t, bool level)
    {
        if (!seen_) {
            seen_ = true;
            first_ = level_ = level;
        }
        else if (level != level_) {
            edges_.push_back(t);
            level_ = level;
        }
    }

    const std::vector<std::uint64_t>& edges() const { return edges_; }

    // the stretches of edges whose half-periods are the stretch's first within `within` of it
    std::vector<Tone> tones(double within = 0.05) const
    {
        std::vector<Tone> out;
        for (std::size_t i = 0, j = 0; i + 1 < edges_.size(); i = j) {
            const double half = static_cast<double>(edges_[i + 1] - edges_[i]);
            for (j = i + 1; j + 1 < edges_.size(); ++j)
                if (std::abs(static_cast<double>(edges_[j + 1] - edges_[j]) - half) > within * half) break;
            if (j - i + 1 >= kToneEdges)
                out.push_back({edges_[i], edges_[j], static_cast<double>(j - i) / 2 * kTicksPerSecond /
                                                         static_cast<double>(edges_[j] - edges_[i])});
        }
        return out;
    }

    // the node from tick `from` up to `to` as a WAV file's bytes, `rate` samples a second
    std::vector<std::uint8_t> wav(std::uint64_t from, std::uint64_t to, unsigned rate) const
    {
        const std::uint64_t samples = (to - from) * rate / kTicksPerSecond;
        std::vector<std::uint8_t> out;
        out.reserve(44 + samples);
        header(out, static_cast<std::uint32_t>(samples), rate);
        // the node at `from`: as first seen, turned once by each edge up to it
        std::size_t next = static_cast<std::size_t>(std::upper_bound(edges_.begin(), edges_.end(), from) - edges_.begin());
        bool level = (seen_ ? first_ : true) != (next % 2 == 1);
        for (std::uint64_t i = 0; i < samples; ++i) {
            const std::uint64_t a = from + i * kTicksPerSecond / rate, b = from + (i + 1) * kTicksPerSecond / rate;
            std::uint64_t high = 0, at = a;
            for (; next < edges_.size() && edges_[next] < b; ++next) {
                if (level) high += edges_[next] - at;
                at = edges_[next];
                level = !level;
            }
            if (level) high += b - at;
            out.push_back(static_cast<std::uint8_t>(kLow + (kHigh - kLow) * high / (b - a)));
        }
        return out;
    }

private:
    static constexpr std::uint64_t kLow = 64, kHigh = 191;   // the samples of a low and of a high node

    static void put(std::vector<std::uint8_t>& out, std::uint32_t value, int bytes)
    {
        for (int i = 0; i < bytes; ++i) out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
    }

    static void tag(std::vector<std::uint8_t>& out, const char* four)
    {
        out.insert(out.end(), four, four + 4);
    }

    // RIFF/WAVE, PCM, one channel, 8 bits
    static void header(std::vector<std::uint8_t>& out, std::uint32_t samples, std::uint32_t rate)
    {
        tag(out, "RIFF");
        put(out, 36 + samples, 4);
        tag(out, "WAVE");
        tag(out, "fmt ");
        put(out, 16, 4);
        put(out, 1, 2);             // PCM
        put(out, 1, 2);             // channels
        put(out, rate, 4);
        put(out, rate, 4);          // bytes a second
        put(out, 1, 2);             // bytes a sample
        put(out, 8, 2);             // bits a sample
        tag(out, "data");
        put(out, samples, 4);
    }

    std::vector<std::uint64_t> edges_;
    bool seen_ = false, first_ = true, level_ = true;
};
