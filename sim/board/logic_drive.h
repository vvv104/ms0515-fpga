// logic_drive: the MiSTer core's floppy drive (rtl/parts/mc5305.sv) on the
// board's XP4 in place of the bench's (drive.h) - run_board's drive=logic.
// The image is served through MiSTer's block interface (block_device.h)
// and mounted as the run starts; the lines are the bench drive's.
//
// The bench's drive runs beside it on the same lines, from the same image,
// as the reference: the report says where their outputs differed and
// whether the two images ended the same.  They differ, by design, while a
// track is loaded after a step, a side's change or the motor's start (the
// bench's drive has every track at once): the bench's drive gives its flux
// pulses there and the logic none yet, so a stretch of difference is a
// pulse, 120 ticks, a run of them each load.  And they may for a few
// bytes after a write: the bench's drive shows the new sector as the gate
// goes off and the logic from its next field, so if the write ends before
// the layout's field does, or right at its CRC's last cell, the cells up
// to the first of the gap after it can differ (kAfterWrite; nothing reads
// there).  The report counts those ticks, and apart from them the ticks
// that differ later than kSettle after a head's move - the time
// test_mc5305 gives the logic, inside the 15 ms the controller waits for
// the head: by design, none.
#pragma once

#include "Vmc5305.h"
#include "block_device.h"
#include "drive.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// After a write's end the two drives' flux may differ up to the first cell
// after the layout's field: within four bytes of the turn (test_mc5305).
inline constexpr std::uint64_t kAfterWrite = 4 * 16 * 240;

// A 512-byte block where two images differ: how many of its bytes, and
// the first of them.
struct BlockDifference {
    std::size_t block;
    int bytes;
    int first;
};

// the blocks where the images differ, as far as the shorter goes
inline std::vector<BlockDifference> differing_blocks(const std::vector<std::uint8_t>& a,
                                                     const std::vector<std::uint8_t>& b)
{
    std::vector<BlockDifference> out;
    const std::size_t blocks = std::min(a.size(), b.size()) / 512;
    for (std::size_t block = 0; block < blocks; ++block) {
        BlockDifference d{block, 0, -1};
        for (int i = 0; i < 512; ++i) {
            if (a[block * 512 + static_cast<std::size_t>(i)] == b[block * 512 + static_cast<std::size_t>(i)])
                continue;
            if (d.bytes++ == 0)
                d.first = i;
        }
        if (d.bytes)
            out.push_back(d);
    }
    return out;
}

class LogicDrive {
public:
    // name: the formatter's, as run_board's layout=; layout: its tracks
    LogicDrive(std::vector<std::uint8_t> image, const std::string& name, drive::Layout layout, bool protect = false)
        : mounted_(image), device_(image), reference_(std::move(image), layout, protect)
    {
        m_.layout = name == "omega" ? 0 : name == "fdz" ? 2 : 1;
        m_.img_size = device_.image().size();
        m_.img_readonly = protect ? 1 : 0;
        m_.img_mounted = 1;
        model_tick(drive::Lines{});
        m_.img_mounted = 0;
    }

    // master tick t with the board's lines as they stand
    drive::Outputs tick(std::uint64_t t, const drive::Lines& in)
    {
        const drive::Outputs o = model_tick(in);
        const drive::Outputs r = reference_.tick(t, in);
        const bool data_was = ref_data_;                // the logic's flux comes a tick later
        ref_data_ = r.data;
        const bool differ = o.index != r.index || o.track0 != r.track0 || o.protect != r.protect ||
                            o.data != data_was || o.ready != r.ready;
        if ((!in.step && step_was_) || in.side != side_was_ || in.motor != motor_was_ || in.select0 != select_was_)
            moved_ = t;
        step_was_ = in.step;
        side_was_ = in.side;
        motor_was_ = in.motor;
        select_was_ = in.select0;
        if (in.write_gate && !gate_was_)
            wrote_ = t;
        gate_was_ = in.write_gate;
        if (differ && t - moved_ > kSettle) {
            if (wrote_ != 0 && t - wrote_ < kAfterWrite)
                ++after_write_;
            else if (late_++ == 0)
                late_first_ = t;
        }
        if (differ) {
            if (run_ == 0) {
                ++stretches_;
                if (first_ == 0) first_ = t;
            }
            ++run_;
            ++ticks_;
            longest_ = std::max(longest_, run_);
        } else {
            run_ = 0;
        }
        return o;
    }

    void report() const
    {
        std::printf("the logic's drive: %d blocks read, %d written; against the bench's: %llu ticks differ "
                    "in %llu stretches (the longest %llu, the first at %.6f s), the images %s\n",
                    device_.reads, device_.writes, static_cast<unsigned long long>(ticks_),
                    static_cast<unsigned long long>(stretches_), static_cast<unsigned long long>(longest_),
                    static_cast<double>(first_) / 120e6, device_.image() == reference_.image() ? "the same" : "differ");
        std::printf("the logic's drive: %llu of those ticks within 4 bytes after a write's end; %llu later than 3 ms "
                    "after a step, a side's change or the motor's start and not after a write",
                    static_cast<unsigned long long>(after_write_), static_cast<unsigned long long>(late_));
        if (late_)
            std::printf(" (the first at %.6f s)", static_cast<double>(late_first_) / 120e6);
        std::printf("\n");
        report_blocks();
    }

    // where the two images differ, and which of them left the image as mounted there
    void report_blocks() const
    {
        const std::vector<BlockDifference> apart = differing_blocks(device_.image(), reference_.image());
        const std::vector<BlockDifference> logic = differing_blocks(device_.image(), mounted_);
        const std::vector<BlockDifference> bench = differing_blocks(reference_.image(), mounted_);
        const auto bytes_of = [](const std::vector<BlockDifference>& in, std::size_t block) {
            for (const BlockDifference& d : in)
                if (d.block == block) return d.bytes;
            return 0;
        };
        std::printf("the logic's drive: %zu blocks not as mounted, the bench's %zu; %zu blocks differ between them\n",
                    logic.size(), bench.size(), apart.size());
        const std::size_t sides = mounted_.size() >= 819200 ? 2 : 1;
        for (std::size_t i = 0; i < apart.size() && i < 10; ++i) {
            const std::size_t block = apart[i].block;
            std::printf("  block %zu (track %zu, side %zu, sector %zu): %d bytes differ, the first at %d; off the "
                        "image as mounted: the logic's %d bytes, the bench's %d\n",
                        block, block / (10 * sides), block / 10 % sides, block % 10 + 1, apart[i].bytes,
                        apart[i].first, bytes_of(logic, block), bytes_of(bench, block));
        }
    }

    const std::vector<std::uint8_t>& image() const { return device_.image(); }

private:
    drive::Outputs model_tick(const drive::Lines& in)
    {
        m_.select0 = in.select0;
        m_.motor = in.motor;
        m_.direction = in.direction;
        m_.step = in.step;
        m_.write_data = in.write_data;
        m_.write_gate = in.write_gate;
        m_.side = in.side;
        device_.serve(m_);
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        drive::Outputs o;
        o.index = m_.index != 0;
        o.track0 = m_.track0 != 0;
        o.protect = m_.protect != 0;
        o.data = m_.data != 0;
        o.ready = m_.ready != 0;
        return o;
    }

    std::unique_ptr<Vmc5305> model_ = std::make_unique<Vmc5305>();
    Vmc5305& m_ = *model_;
    std::vector<std::uint8_t> mounted_;                 // the image as the run began
    BlockDevice<Vmc5305> device_;
    drive::Mc5305 reference_;
    static constexpr std::uint64_t kSettle = 3000 * 120;    // 3 ms, as test_mc5305's
    std::uint64_t ticks_ = 0, stretches_ = 0, longest_ = 0, run_ = 0, first_ = 0;
    std::uint64_t moved_ = 0, late_ = 0, late_first_ = 0;
    std::uint64_t wrote_ = 0, after_write_ = 0;         // the last write's end; the ticks that differ after one
    bool gate_was_ = true;
    bool ref_data_ = true;
    bool step_was_ = true, side_was_ = true, motor_was_ = true, select_was_ = true;
};
