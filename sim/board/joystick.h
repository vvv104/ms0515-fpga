// joystick: the owner's joystick on the МС 7007's port - five switches to
// ground on port B of D82 (ДКЛ08..12 L, XP8.10..14), an open line read as 1:
// right, left, down, up, fire, the Kempston order (found on the owner's
// machine through the 1991 SABOT2, which reads 177542; the emulator's
// docs/hardware).  A run's file says which are held and when: a line
// "MS LINES", the machine time in ms and the lines held from then on, octal
// (1 right, 2 left, 4 down, 010 up, 020 fire; 0 lets go); '#' begins a
// comment.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

class Joystick {
public:
    static constexpr std::uint64_t kTicksPerMs = 120'000;
    static constexpr unsigned kLines = 5;

    explicit Joystick(std::istream& file)
    {
        std::string line;
        while (std::getline(file, line)) {
            std::istringstream s(line.substr(0, line.find('#')));
            std::uint64_t ms = 0;
            std::string lines;
            if (s >> ms >> lines)
                at_.emplace_back(ms * kTicksPerMs,
                                 static_cast<unsigned>(std::strtoul(lines.c_str(), nullptr, 8)) & ((1u << kLines) - 1));
        }
        std::sort(at_.begin(), at_.end());
    }

    // the lines held at master tick t, bit 0 the first line
    unsigned held(std::uint64_t t) const
    {
        const auto after = std::upper_bound(at_.begin(), at_.end(), std::make_pair(t, ~0u));
        return after == at_.begin() ? 0 : (after - 1)->second;
    }

private:
    std::vector<std::pair<std::uint64_t, unsigned>> at_;
};
