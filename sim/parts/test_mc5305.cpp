// mc5305: the floppy drive as logic, against the bench's drive (sim/board/
// drive.h) tick by tick: the index, track 0, ready and the flux pulses on
// the same lines, from an image served through MiSTer's block interface.
// After the head moves, or the side changes, the logic loads the track and
// works its way to the head's place in the turn; the pulses are compared
// again once it has (kSettle, inside the 15 ms a drive's head takes to
// settle, which the controller waits).
#include <doctest/doctest.h>

#include "Vmc5305.h"
#include "../board/drive.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr std::uint64_t kUs = 120;
constexpr std::uint64_t kSettle = 3000 * kUs;

// an image whose every byte tells where it is
std::vector<std::uint8_t> image(int sides)
{
    std::vector<std::uint8_t> im(static_cast<std::size_t>(sides) * drive::kTracks * drive::kSectors *
                                 drive::kSectorBytes);
    for (std::size_t i = 0; i < im.size(); ++i)
        im[i] = static_cast<std::uint8_t>(i * 7 + i / 512 * 13 + i / 5120);
    return im;
}

class Bench {
public:
    Bench(std::vector<std::uint8_t> im, int layout)
        : image_(std::move(im)), ref_(image_, layout == 0 ? drive::omega() : layout == 1 ? drive::vvv() : drive::fdz())
    {
        m_.layout = static_cast<CData>(layout);
        m_.img_size = image_.size();
        m_.img_readonly = 0;
        m_.select0 = 1;
        m_.motor = 1;
        m_.direction = 1;
        m_.step = 1;
        m_.write_gate = 1;
        m_.side = 1;
        m_.sd_ack = 0;
        m_.sd_buff_wr = 0;
        m_.img_mounted = 1;
        tick();
        m_.img_mounted = 0;
    }

    // ticks with the lines as set; the outputs compared while `compare`
    void run(std::uint64_t ticks, bool compare)
    {
        for (std::uint64_t i = 0; i < ticks; ++i) {
            tick();
            const drive::Outputs o = ref_.tick(t_, lines_);
            if (!compare)
                continue;
            ++compared;
            const bool same = o.index == (m_.index != 0) && o.track0 == (m_.track0 != 0) &&
                              o.ready == (m_.ready != 0) && o.protect == (m_.protect != 0) &&
                              o.data == (m_.data != 0);
            if (!same && ++differ <= 5)
                MESSAGE("tick " << t_ << ": ref index " << o.index << " track0 " << o.track0 << " ready " << o.ready
                                << " data " << o.data << "; logic " << int(m_.index) << " " << int(m_.track0) << " "
                                << int(m_.ready) << " " << int(m_.data));
        }
    }

    void select(bool on) { lines_.select0 = !on; m_.select0 = lines_.select0; }
    void motor(bool on) { lines_.motor = !on; m_.motor = lines_.motor; }
    void side(int s) { lines_.side = s == 0; m_.side = lines_.side; }
    // one step pulse (ШАГ L low 4 us), inward or out
    void step(bool in)
    {
        lines_.direction = !in;
        m_.direction = lines_.direction;
        run(10 * kUs, false);
        lines_.step = false;
        m_.step = 0;
        run(4 * kUs, false);
        lines_.step = true;
        m_.step = 1;
        run(3000 * kUs, false);
    }
    int head() const { return ref_.head(); }

    std::uint64_t compared = 0, differ = 0;
    int loads = 0;

private:
    // a master tick, the block device answering as MiSTer's does: a read
    // asked for is acknowledged, its 512 bytes written, the ack let go
    void tick()
    {
        serve();
        m_.clk = 0;
        m_.eval();
        m_.clk = 1;
        m_.eval();
        ++t_;
    }

    void serve()
    {
        m_.sd_buff_wr = 0;
        if (byte_ < 0) {
            if (m_.sd_rd && !m_.sd_ack) {
                byte_ = 0;
                lba_ = m_.sd_lba;
                m_.sd_ack = 1;
                ++loads;
            } else {
                m_.sd_ack = 0;
            }
            return;
        }
        if (byte_ < 512) {
            const std::size_t at = static_cast<std::size_t>(lba_) * 512 + static_cast<std::size_t>(byte_);
            m_.sd_buff_addr = static_cast<SData>(byte_);
            m_.sd_buff_dout = at < image_.size() ? image_[at] : 0;
            m_.sd_buff_wr = 1;
            ++byte_;
            return;
        }
        m_.sd_ack = 0;
        byte_ = -1;
    }

    std::vector<std::uint8_t> image_;
    drive::Mc5305 ref_;
    drive::Lines lines_;
    std::unique_ptr<Vmc5305> model_ = std::make_unique<Vmc5305>();
    Vmc5305& m_ = *model_;
    std::uint64_t t_ = 0;
    int byte_ = -1;
    std::uint32_t lba_ = 0;
};

constexpr std::uint64_t kTurn = drive::Mc5305::kTurnTicks;

}  // namespace

TEST_CASE("mc5305: unselected, every line let go")
{
    Bench b(image(1), 1);
    b.motor(true);
    b.run(kTurn / 4, true);
    CHECK(b.differ == 0);
}

TEST_CASE("mc5305: the turns of track 0 from the motor's start, as the bench's drive")
{
    for (const int layout : {0, 1, 2}) {
        INFO("layout " << layout);
        Bench b(image(2), layout);
        b.select(true);
        b.run(kSettle, false);              // the track loaded while the motor is still
        b.motor(true);
        b.run(3 * kTurn + kTurn / 2, true); // index, ready from the third, the flux
        CHECK(b.compared > 0);
        CHECK(b.differ == 0);
    }
}

TEST_CASE("mc5305: the head stepped in and out, the side changed, a single-sided image")
{
    Bench b(image(1), 1);
    b.select(true);
    b.motor(true);
    b.run(kSettle, false);
    b.step(true);
    b.step(true);
    REQUIRE(b.head() == 2);
    b.run(kSettle, false);
    b.run(kTurn / 3, true);
    b.side(1);                              // no second side: side 0 again
    b.run(kSettle, false);
    b.run(kTurn / 5, true);
    b.step(false);
    REQUIRE(b.head() == 1);
    b.run(kSettle, false);
    b.run(kTurn / 3, true);
    CHECK(b.differ == 0);
    // tracks 0, 1, 2 and 1 again, ten blocks each; the side alone loads nothing here
    CHECK(b.loads == 4 * drive::kSectors);
}
