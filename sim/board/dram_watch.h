// dram_watch: the board's dynamic RAM seen from its nets - the 16
// КР565РУ5 on А0..А7 H, ВАС L (-RAS), ВАК L (-CAS), ЗП МЛ L / ЗП СТ L
// (-WE of the low and high byte), МК00..15 H in, Д00..15 H out.  Every
// write goes into a shadow of the cells; every read is checked against it.
// The strobes are taken as the chips take them (rtl/chips/kr565ru5.sv):
// the row the tick before -RAS falls, the column as -CAS falls.
#pragma once

#include "Vns4.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

class DramWatch {
public:
    struct Stats {
        std::uint64_t writes = 0, reads = 0, checked = 0, differ = 0, fills = 0;
        std::uint64_t video_reads = 0, video_fills = 0, video_nonzero = 0;
    };

    // where the 047777 words went and where the video reads, as cells
    // (row << 8 | column): how many of each, and of both
    void print_cells() const
    {
        std::size_t fills = 0, video = 0, both = 0;
        std::size_t fill_lo = 65536, fill_hi = 0, video_lo = 65536, video_hi = 0;
        for (std::size_t c = 0; c < 65536; ++c) {
            if (fill_cells_[c]) { ++fills; fill_lo = std::min(fill_lo, c); fill_hi = std::max(fill_hi, c); }
            if (video_cells_[c]) { ++video; video_lo = std::min(video_lo, c); video_hi = std::max(video_hi, c); }
            if (fill_cells_[c] && video_cells_[c]) ++both;
        }
        std::printf("047777 cells %zu (%06zo..%06zo), video cells %zu (%06zo..%06zo), both %zu\n", fills,
                    fill_lo, fill_hi, video, video_lo, video_hi, both);
    }

    // print the strobes, the address and МК tick by tick through the first
    // `cycles` RAS cycles that write
    void trace(int cycles) { trace_left_ = cycles; }

    void see(const Vns4& m)
    {
        const std::uint8_t a = address(m);
        const bool ras = m.n_VAS_L != 0, cas = m.n_VAK_L != 0;
        const bool we_lo = !m.n_ZP_ML_L, we_hi = !m.n_ZP_ST_L;
        if (trace_left_ > 0) {
            if (!ras) {
                lines_.push_back({ras, cas, we_lo, we_hi, a, data_in(m), data_out(m),
                                  static_cast<std::uint8_t>((m.n_VB1_H & 1u) << 1 | (m.n_VB0_H & 1u)),
                                  cpu_address(m)});
                wrote_ = wrote_ || we_lo || we_hi;
            } else if (!ras_was_) {
                if (wrote_) {
                    std::printf("-- a RAS cycle that writes:\n");
                    for (const Line& l : lines_)
                        std::printf("RAS %d CAS %d WE lo %d hi %d  A %03o  MK %06o  D %06o  VB %d  AD %06o\n",
                                    l.ras, l.cas, l.we_lo, l.we_hi, l.a, l.mk, l.d, l.vb, l.ad);
                    --trace_left_;
                }
                lines_.clear();
                wrote_ = false;
            }
        }
        if (!ras && ras_was_)
            row_ = a_was_;
        if (!ras && !cas && cas_was_) {
            column_ = a;
            video_ = (m.n_VB1_H & 1u) != 0;
            if (we_lo || we_hi)
                write(m, we_lo, we_hi);
            else
                reading_ = true;
        } else if (!ras && !cas) {
            const bool lo = we_lo && !we_lo_was_, hi = we_hi && !we_hi_was_;
            if (lo || hi)
                write(m, lo, hi);                       // late write
        }
        if (reading_ && !cas)
            last_ = data_out(m);
        if (reading_ && (cas || ras)) {
            check();
            reading_ = false;
        }
        a_was_ = a;
        ras_was_ = ras;
        cas_was_ = cas;
        we_lo_was_ = we_lo;
        we_hi_was_ = we_hi;
    }

    const Stats& stats() const { return stats_; }

    // the cell (row << 8 | column) the last write went to
    std::size_t last_write() const { return last_write_; }
    // and the cell the last video read came from
    std::size_t last_video() const { return last_video_; }

    // the word at a cell, and which of its bytes were ever written
    std::uint16_t word(std::size_t cell) const { return cells_[cell]; }
    std::uint8_t known(std::size_t cell) const { return known_[cell]; }

private:
    static std::uint8_t address(const Vns4& m)
    {
        const CData bits[8] = {m.n_A0_H, m.n_A1_H, m.n_A2_H, m.n_A3_H,
                               m.n_A4_H, m.n_A5_H, m.n_A6_H, m.n_A7_H};
        std::uint8_t v = 0;
        for (int i = 0; i < 8; ++i)
            v = static_cast<std::uint8_t>(v | (bits[i] & 1u) << i);
        return v;
    }
    static std::uint16_t data_in(const Vns4& m)
    {
        const CData bits[16] = {m.n_MK00_H, m.n_MK01_H, m.n_MK02_H, m.n_MK03_H, m.n_MK04_H, m.n_MK05_H,
                                m.n_MK06_H, m.n_MK07_H, m.n_MK08_H, m.n_MK09_H, m.n_MK10_H, m.n_MK11_H,
                                m.n_MK12_H, m.n_MK13_H, m.n_MK14_H, m.n_MK15_H};
        return pack(bits);
    }
    static std::uint16_t data_out(const Vns4& m)
    {
        const CData bits[16] = {m.n_D00_H, m.n_D01_H, m.n_D02_H, m.n_D03_H, m.n_D04_H, m.n_D05_H,
                                m.n_D06_H, m.n_D07_H, m.n_D08_H, m.n_D09_H, m.n_D10_H, m.n_D11_H,
                                m.n_D12_H, m.n_D13_H, m.n_D14_H, m.n_D15_H};
        return pack(bits);
    }
    static std::uint16_t cpu_address(const Vns4& m)
    {
        const CData bits[16] = {m.n_AD00_H, m.n_AD01_H, m.n_AD02_H, m.n_AD03_H, m.n_AD04_H, m.n_AD05_H,
                                m.n_AD06_H, m.n_AD07_H, m.n_AD08_H, m.n_AD09_H, m.n_AD10_H, m.n_AD11_H,
                                m.n_AD12_H, m.n_AD13_H, m.n_AD14_H, m.n_AD15_H};
        return pack(bits);
    }
    static std::uint16_t pack(const CData (&bits)[16])
    {
        std::uint16_t v = 0;
        for (int i = 0; i < 16; ++i)
            v = static_cast<std::uint16_t>(v | (bits[i] & 1u) << i);
        return v;
    }

    std::size_t cell() const { return static_cast<std::size_t>(row_) << 8 | column_; }

    void write(const Vns4& m, bool lo, bool hi)
    {
        last_write_ = cell();
        const std::uint16_t d = data_in(m);
        std::uint16_t& w = cells_[cell()];
        if (lo) w = static_cast<std::uint16_t>((w & 0xFF00) | (d & 0x00FF));
        if (hi) w = static_cast<std::uint16_t>((w & 0x00FF) | (d & 0xFF00));
        known_[cell()] = static_cast<std::uint8_t>(known_[cell()] | (lo ? 1 : 0) | (hi ? 2 : 0));
        ++stats_.writes;
        if (lo && hi && d == 0x47FF) {
            ++stats_.fills;
            fill_cells_[cell()] = 1;
        }
    }

    void check()
    {
        ++stats_.reads;
        if (video_) {
            ++stats_.video_reads;
            last_video_ = cell();
            video_cells_[cell()] = 1;
            if (last_ == 0x47FF) ++stats_.video_fills;
            if (last_ != 0) ++stats_.video_nonzero;
        }
        if (known_[cell()] != 3)
            return;
        ++stats_.checked;
        if (last_ != cells_[cell()] && ++stats_.differ <= 5)
            std::printf("read row %03o column %03o: %06o, written %06o\n", row_, column_, last_,
                        cells_[cell()]);
    }

    std::vector<std::uint16_t> cells_ = std::vector<std::uint16_t>(65536, 0);
    std::vector<std::uint8_t> known_ = std::vector<std::uint8_t>(65536, 0);
    std::vector<std::uint8_t> fill_cells_ = std::vector<std::uint8_t>(65536, 0);
    std::vector<std::uint8_t> video_cells_ = std::vector<std::uint8_t>(65536, 0);
    bool video_ = false;
    std::uint8_t a_was_ = 0, row_ = 0, column_ = 0;
    std::size_t last_write_ = 0, last_video_ = 0;
    bool ras_was_ = true, cas_was_ = true, we_lo_was_ = false, we_hi_was_ = false, reading_ = false;
    std::uint16_t last_ = 0;
    Stats stats_;

    struct Line {
        bool ras, cas, we_lo, we_hi;
        std::uint8_t a;
        std::uint16_t mk, d;
        std::uint8_t vb;
        std::uint16_t ad;
    };
    int trace_left_ = 0;
    bool wrote_ = false;
    std::vector<Line> lines_;
};
