// snapshot: the bench's own parts (the drive, the keyboard stub, the run's
// counters) into and out of a Verilator save file, beside the board model's
// state, so a run goes on from where an earlier one stopped instead of
// living the same seconds again.  A part lists its fields once, in
// state(archive); the archive writes them or reads them back.
#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

// Any stream with the Verilator serialize operators (VerilatedSerialize, or
// a test's own): every field as a fixed-width integer.
template <class Out>
struct SaveArchive {
    Out& os;

    template <class T>
    void operator()(const T& v)
    {
        if constexpr (std::is_same_v<T, bool>) {
            os << v;
        } else if constexpr (std::is_integral_v<T> && sizeof(T) <= 4) {
            const std::uint32_t u = static_cast<std::uint32_t>(v);
            os << u;
        } else if constexpr (std::is_integral_v<T>) {
            const std::uint64_t u = static_cast<std::uint64_t>(v);
            os << u;
        } else {
            static_assert(sizeof(T) == 0, "a field the archive does not know");
        }
    }
    void bytes(const std::vector<std::uint8_t>& v)
    {
        (*this)(static_cast<std::uint64_t>(v.size()));
        for (const std::uint8_t b : v) (*this)(b);
    }
};

template <class In>
struct LoadArchive {
    In& is;

    template <class T>
    void operator()(T& v)
    {
        if constexpr (std::is_same_v<T, bool>) {
            is >> v;
        } else if constexpr (std::is_integral_v<T> && sizeof(T) <= 4) {
            std::uint32_t u = 0;
            is >> u;
            v = static_cast<T>(u);
        } else if constexpr (std::is_integral_v<T>) {
            std::uint64_t u = 0;
            is >> u;
            v = static_cast<T>(u);
        } else {
            static_assert(sizeof(T) == 0, "a field the archive does not know");
        }
    }
    void bytes(std::vector<std::uint8_t>& v)
    {
        std::uint64_t n = 0;
        (*this)(n);
        v.resize(static_cast<std::size_t>(n));
        for (std::uint8_t& b : v) (*this)(b);
    }
};
