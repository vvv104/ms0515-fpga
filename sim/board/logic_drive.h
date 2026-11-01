// logic_drive: the MiSTer core's floppy drive (rtl/parts/mc5305.sv) on the
// board's XP4 in place of the bench's (drive.h) - run_board's drive=logic.
// The image is served through MiSTer's block interface (block_device.h)
// and mounted as the run starts; the lines are the bench drive's.
//
// The bench's drive runs beside it on the same lines, from the same image,
// as the reference: the report says where their outputs differed and
// whether the two images ended the same.  They differ, by design, while a
// track is loaded after a step (the bench's drive has every track at once),
// a stretch of some thousand ticks each.
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

class LogicDrive {
public:
    // name: the formatter's, as run_board's layout=; layout: its tracks
    LogicDrive(std::vector<std::uint8_t> image, const std::string& name, drive::Layout layout, bool protect = false)
        : device_(image), reference_(std::move(image), layout, protect)
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
        const bool differ = o.index != r.index || o.track0 != r.track0 || o.protect != r.protect ||
                            o.data != r.data || o.ready != r.ready;
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
    BlockDevice<Vmc5305> device_;
    drive::Mc5305 reference_;
    std::uint64_t ticks_ = 0, stretches_ = 0, longest_ = 0, run_ = 0, first_ = 0;
};
