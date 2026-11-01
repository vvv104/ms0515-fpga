// logic_drive: the MiSTer core's floppy drive (rtl/parts/mc5305.sv) on the
// board's XP4 in place of the bench's (drive.h) - run_board's drive=logic.
// The image is served through MiSTer's block interface (block_device.h)
// and mounted as the run starts; the lines are the bench drive's.
#pragma once

#include "Vmc5305.h"
#include "block_device.h"
#include "drive.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class LogicDrive {
public:
    // layout: the formatter's name, as run_board's layout=
    LogicDrive(std::vector<std::uint8_t> image, const std::string& layout, bool protect = false)
        : device_(std::move(image))
    {
        m_.layout = layout == "omega" ? 0 : layout == "fdz" ? 2 : 1;
        m_.img_size = device_.image().size();
        m_.img_readonly = protect ? 1 : 0;
        m_.img_mounted = 1;
        drive::Lines idle;
        tick(idle);
        m_.img_mounted = 0;
    }

    // a master tick with the board's lines as they stand
    drive::Outputs tick(const drive::Lines& in)
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

    const std::vector<std::uint8_t>& image() const { return device_.image(); }
    int reads() const { return device_.reads; }
    int writes() const { return device_.writes; }

private:
    std::unique_ptr<Vmc5305> model_ = std::make_unique<Vmc5305>();
    Vmc5305& m_ = *model_;
    BlockDevice<Vmc5305> device_;
};
