// keyboard_stub: the bench's stand-in for the МС 7004 on XS2, until the
// keyboard has a model of its own - it only answers ROM-B's question.
// ROM-B (173570..173740) sets the USART to 8N1 x16, sends AB (an LK201's
// "request keyboard ID") and waits for 01 then 00; without them it prints
// «Клавиатура не готова» and tries eleven times, about a second each,
// before it goes on to the drive.  The stub hears every byte on ВЫХКЛ L
// (XS2.2) and answers AB with 01 00 on ВХКЛ L (XS2.3).
//
// The line's sense at XS2, as the board's driver (КР1102АП15, inverting)
// and receiver (КР559ИП11) make it: idle 0 (the negative mark), a start
// bit 1, a data bit 1 for a 0.  Its speed is the board's: the timer's
// 2 MHz / 26 (ROM-B's divisor) / 16 (the USART's x16), about 4808 baud.
#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace keyboard {

class Stub {
public:
    static constexpr std::uint64_t kBitTicks = 120ull * 26 * 16 / 2;    // 120 MHz ticks at 2 MHz / 26 / 16
    static constexpr std::uint64_t kAnswerDelayTicks = 120ull * 1000;  // the keyboard's 1 ms to think

    // the line from the board at master tick t; returns the line to it
    bool tick(std::uint64_t t, bool from_board)
    {
        hear(t, from_board);
        return say(t);
    }

    const std::vector<std::uint8_t>& heard() const { return heard_; }

private:
    void hear(std::uint64_t t, bool line)
    {
        if (!in_frame_) {
            if (line && !line_was_) {               // a start bit
                in_frame_ = true;
                start_ = t;
                byte_ = 0;
                bit_ = 0;
            }
        } else if (t >= start_ + static_cast<std::uint64_t>(bit_ + 1) * kBitTicks + kBitTicks / 2) {
            if (bit_ < 8) {
                if (!line) byte_ = static_cast<std::uint8_t>(byte_ | 1 << bit_);
                ++bit_;
            } else {                                // the stop bit's middle
                in_frame_ = false;
                heard_.push_back(byte_);
                if (byte_ == 0xAB) answer(t);
            }
        }
        line_was_ = line;
    }

    void answer(std::uint64_t t)
    {
        const std::uint64_t at = t + kAnswerDelayTicks;
        queue(0x01, at);
        queue(0x00, at + 11 * kBitTicks);
    }

    // a frame's levels a bit at a time, from tick `at`
    void queue(std::uint8_t b, std::uint64_t at)
    {
        bits_.push_back({at, true});
        for (int i = 0; i < 8; ++i)
            bits_.push_back({at + static_cast<std::uint64_t>(i + 1) * kBitTicks, ((b >> i) & 1) == 0});
        bits_.push_back({at + 9 * kBitTicks, false});
    }

    bool say(std::uint64_t t)
    {
        while (!bits_.empty() && bits_.front().at <= t) {
            level_ = bits_.front().level;
            bits_.pop_front();
        }
        return level_;
    }

    struct Bit {
        std::uint64_t at;
        bool level;
    };

    bool line_was_ = false, in_frame_ = false, level_ = false;
    std::uint64_t start_ = 0;
    std::uint8_t byte_ = 0;
    int bit_ = 0;
    std::vector<std::uint8_t> heard_;
    std::deque<Bit> bits_;
};

}  // namespace keyboard
