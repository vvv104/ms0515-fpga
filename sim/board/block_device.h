// block_device: MiSTer's block interface (hps_io's sd_*: 512-byte blocks)
// over a disk image, as the bench serves it to the logic's drive
// (rtl/parts/mc5305.sv).  A read asked for (sd_rd) is acknowledged and its
// 512 bytes written into the core's buffer a tick each; a write (sd_wr) is
// acknowledged and its bytes taken off sd_buff_din, each a tick after its
// address is put out; then the acknowledgement is let go.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

template <class V>
class BlockDevice {
public:
    explicit BlockDevice(std::vector<std::uint8_t> image) : image_(std::move(image)) {}

    // the interface's inputs for the next master tick
    void serve(V& m)
    {
        m.sd_buff_wr = 0;
        if (writing_ >= 0)
            take(m);
        else if (reading_ >= 0)
            give(m);
        else if (m.sd_rd && !m.sd_ack)
            begin(m, reading_, reads);
        else if (m.sd_wr && !m.sd_ack)
            begin(m, writing_, writes);
        else
            m.sd_ack = 0;
    }

    const std::vector<std::uint8_t>& image() const { return image_; }
    int reads = 0, writes = 0;

private:
    void begin(V& m, int& byte, int& count)
    {
        byte = 0;
        lba_ = m.sd_lba;
        m.sd_ack = 1;
        ++count;
    }

    void give(V& m)
    {
        if (reading_ == 512) {
            m.sd_ack = 0;
            reading_ = -1;
            return;
        }
        const std::size_t at = static_cast<std::size_t>(lba_) * 512 + static_cast<std::size_t>(reading_);
        m.sd_buff_addr = static_cast<std::remove_reference_t<decltype(m.sd_buff_addr)>>(reading_);
        m.sd_buff_dout = at < image_.size() ? image_[at] : 0;
        m.sd_buff_wr = 1;
        ++reading_;
    }

    void take(V& m)
    {
        if (writing_ > 0) {
            const std::size_t at = static_cast<std::size_t>(lba_) * 512 + static_cast<std::size_t>(writing_ - 1);
            if (at < image_.size()) image_[at] = m.sd_buff_din;
        }
        if (writing_ == 512) {
            m.sd_ack = 0;
            writing_ = -1;
            return;
        }
        m.sd_buff_addr = static_cast<std::remove_reference_t<decltype(m.sd_buff_addr)>>(writing_);
        ++writing_;
    }

    std::vector<std::uint8_t> image_;
    std::uint32_t lba_ = 0;
    int reading_ = -1, writing_ = -1;
};
