// cpu_writes: the processor's writes as the board's bus shows them - the
// address on АД00..15 H as ПВАС L falls, the bytes strobed by ПМЛБ L /
// ПСТБ L, the data on МК00..15 H as ПВАК L rises, the write taken when
// ПВАС L rises again.  A test ROM leaves its results in RAM; this is how
// the bench reads them back without knowing where the dispatcher put
// them in the DRAM.
#pragma once

#include <cstdint>
#include <map>
#include <optional>

class CpuWrites {
public:
    template <class V>
    void see(const V& m)
    {
        const bool ras = m.n_PVAS_L != 0;
        if (!ras && ras_was_) {
            at_ = word(m.n_AD00_H, m.n_AD01_H, m.n_AD02_H, m.n_AD03_H, m.n_AD04_H, m.n_AD05_H, m.n_AD06_H,
                       m.n_AD07_H, m.n_AD08_H, m.n_AD09_H, m.n_AD10_H, m.n_AD11_H, m.n_AD12_H, m.n_AD13_H,
                       m.n_AD14_H, m.n_AD15_H);
            lo_ = hi_ = false;
        }
        if (!ras) {
            lo_ = lo_ || !m.n_PMLB_L;
            hi_ = hi_ || !m.n_PSTB_L;
            if (m.n_PVAK_L && !cas_was_ && (lo_ || hi_))
                data_ = word(m.n_MK00_H, m.n_MK01_H, m.n_MK02_H, m.n_MK03_H, m.n_MK04_H, m.n_MK05_H, m.n_MK06_H,
                             m.n_MK07_H, m.n_MK08_H, m.n_MK09_H, m.n_MK10_H, m.n_MK11_H, m.n_MK12_H, m.n_MK13_H,
                             m.n_MK14_H, m.n_MK15_H);
        }
        if (ras && !ras_was_ && (lo_ || hi_)) {
            const unsigned even = at_ & ~1u;
            if (lo_) bytes_[even] = static_cast<std::uint8_t>(data_ & 0xFF);
            if (hi_) bytes_[even + 1] = static_cast<std::uint8_t>(data_ >> 8);
            ++count_;
        }
        ras_was_ = ras;
        cas_was_ = m.n_PVAK_L != 0;
    }

    // the byte last written at an address, if any was
    std::optional<std::uint8_t> byte(unsigned address) const
    {
        const auto it = bytes_.find(address);
        if (it == bytes_.end()) return std::nullopt;
        return it->second;
    }

    // the word at an even address, if both its bytes were written
    std::optional<std::uint16_t> word_at(unsigned address) const
    {
        const auto lo = byte(address), hi = byte(address + 1);
        if (!lo || !hi) return std::nullopt;
        return static_cast<std::uint16_t>(*lo | *hi << 8);
    }

    std::uint64_t count() const { return count_; }

private:
    template <class... Bits>
    static unsigned word(Bits... bits)
    {
        unsigned v = 0, n = 0;
        ((v |= (bits & 1u) << n++), ...);
        return v;
    }

    std::map<unsigned, std::uint8_t> bytes_;
    std::uint64_t count_ = 0;
    unsigned at_ = 0, data_ = 0;
    bool lo_ = false, hi_ = false, ras_was_ = true, cas_was_ = true;
};
