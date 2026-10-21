// run_board: the generated НС4 board from power-on for a stretch of machine
// time - the processor's fetches checked against the emulator's, and the
// picture taken off the video DAC's digital inputs, frame by frame.
//
//   run_board <machine ms> <folder for the frames> [every Nth frame] [video]
//
// Every Nth frame is saved and compared with the emulator's screen; with
// `video`, every frame is saved as well, into <folder>/video, with the
// master tick each began at in video/frames.txt - tools/frames_to_video.py
// makes them a film in the machine's own time.
//
// A frame is the lines between two frame syncs (КСИ L), a line the samples
// at each ТС15 H rise from the line sync (ССИ L): the blue, red and green
// levels (СЦ, КЦ, ЗЦ H) and the brightness line (D69.9).  Frames are BMP
// files, the whole line with its blanking (960 x the frame's lines).
#include <verilated.h>

#include "Vns4.h"
#include "power.h"
#include "dram_watch.h"
#include "screen_compare.h"

extern "C" {
#include <ms0515/core/board.h>
#include <ms0515/core/cpu.h>
#include <ms0515/core/memory.h>
}

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr int kMasterMhz = 120;
constexpr int kLineSamples = 960;           // 64 us at 15 MHz

std::uint16_t mk(const Vns4& m)
{
    const CData bits[16] = {m.n_MK00_H, m.n_MK01_H, m.n_MK02_H, m.n_MK03_H, m.n_MK04_H, m.n_MK05_H,
                            m.n_MK06_H, m.n_MK07_H, m.n_MK08_H, m.n_MK09_H, m.n_MK10_H, m.n_MK11_H,
                            m.n_MK12_H, m.n_MK13_H, m.n_MK14_H, m.n_MK15_H};
    std::uint16_t w = 0;
    for (int i = 0; i < 16; ++i)
        w = static_cast<std::uint16_t>(w | (bits[i] & 1u) << i);
    return w;
}

// one frame as samples: bits 0 blue, 1 red, 2 green, 3 brightness
struct Frame {
    std::vector<std::vector<std::uint8_t>> lines;
};

void write_bmp(const std::string& path, const Frame& f)
{
    const int w = kLineSamples, h = static_cast<int>(f.lines.size());
    if (h == 0) return;
    const int row = w * 3;
    const std::uint32_t size = static_cast<std::uint32_t>(54 + row * h);
    std::vector<std::uint8_t> out(size, 0);
    auto put32 = [&](std::size_t at, std::uint32_t v) {
        for (int i = 0; i < 4; ++i) out[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    out[0] = 'B'; out[1] = 'M';
    put32(2, size); put32(10, 54); put32(14, 40);
    put32(18, static_cast<std::uint32_t>(w)); put32(22, static_cast<std::uint32_t>(-h));
    out[26] = 1; out[28] = 24;
    for (int y = 0; y < h; ++y) {
        const std::vector<std::uint8_t>& line = f.lines[static_cast<std::size_t>(y)];
        for (int x = 0; x < w && x < static_cast<int>(line.size()); ++x) {
            const std::uint8_t s = line[static_cast<std::size_t>(x)];
            const std::uint8_t on = (s & 8) ? 255 : 170;
            const std::size_t at = 54 + static_cast<std::size_t>(y * row + x * 3);
            out[at + 0] = (s & 1) ? on : 0;          // BMP is B, G, R
            out[at + 1] = (s & 4) ? on : 0;
            out[at + 2] = (s & 2) ? on : 0;
        }
    }
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(out.data()),
                                                static_cast<std::streamsize>(out.size()));
}

// the emulator from reset with ROM-B, stepped one instruction at a time
class Emulator {
public:
    explicit Emulator(const std::string& rom_path)
    {
        std::ifstream f(rom_path, std::ios::binary);
        const std::vector<char> rom{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
        board_init(b_.get());
        board_load_rom(b_.get(), reinterpret_cast<const std::uint8_t*>(rom.data()),
                       static_cast<std::uint32_t>(rom.size()));
        board_reset(b_.get());
    }
    std::uint16_t next()
    {
        const std::uint16_t pc = b_->cpu.r[7];
        cpu_step(&b_->cpu);
        return pc;
    }
    // the most common byte values at even (pixels) and odd (attributes)
    // addresses, and the first words
    void print_vram_summary() const
    {
        const std::span<const std::uint8_t> vram(board_get_vram(b_.get()), MEM_VRAM_SIZE);
        for (int half = 0; half < 2; ++half) {
            std::array<int, 256> seen{};
            for (std::size_t i = static_cast<std::size_t>(half); i < vram.size(); i += 2)
                ++seen[vram[i]];
            std::printf("%s bytes:", half ? "odd " : "even");
            for (int k = 0; k < 4; ++k) {
                const auto top = std::ranges::max_element(seen);
                std::printf(" %02x x%d", static_cast<int>(top - seen.begin()), *top);
                *top = -1;
            }
            std::printf("\n");
        }
        for (std::size_t i = 0; i < 16; ++i)
            std::printf("%02x ", vram[i]);
        std::printf("\n");
    }
    // the board's frame against the emulator's picture of its video memory
    ScreenCompare screen(const Frame& f) const
    {
        return compare_screen(f.lines, board_get_vram(b_.get()), board_is_hires(b_.get()),
                              board_get_border_color(b_.get()));
    }
    // the bytes of the emulator's video memory that are not zero
    std::size_t vram_used() const
    {
        const std::span<const std::uint8_t> vram(board_get_vram(b_.get()), MEM_VRAM_SIZE);
        return static_cast<std::size_t>(std::ranges::count_if(vram, [](std::uint8_t v) { return v != 0; }));
    }

private:
    std::unique_ptr<ms0515_board_t> b_ = std::make_unique<ms0515_board_t>();
};

// The board's fetches against the emulator's.  The board waits as the
// machine does - a USART's transmitter takes a character's time to go
// out, the emulator's none - so where the board turns away into a loop
// it has already run (an address among its last fetches), the emulator
// waits for it to come out at the emulator's next address.
class Lockstep {
public:
    enum Result { Same, Waiting, Apart };
    static constexpr std::uint64_t kLongestWait = 4'000'000;     // ~10 s of the machine

    std::uint16_t want = 0;
    std::uint64_t waits = 0, waited = 0;

    Result see(std::uint16_t at, Emulator& emu)
    {
        if (waiting_ == 0)
            want = emu.next();
        Result r = Same;
        if (at != want) {
            const bool loop = std::find(recent_.begin(), recent_.end(), at) != recent_.end();
            if (waiting_ == 0 && !loop)
                return Apart;
            if (waiting_ == 0)
                ++waits;
            if (++waiting_ > kLongestWait)
                return Apart;
            ++waited;
            r = Waiting;
        } else if (waiting_ != 0) {
            waiting_ = 0;
        }
        recent_[next_++ % recent_.size()] = at;
        return r;
    }

private:
    std::uint64_t waiting_ = 0;
    std::array<std::uint16_t, 64> recent_{};
    std::size_t next_ = 0;
};

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: run_board <machine ms> <folder> [every Nth frame] [video]\n");
        return 2;
    }
    const std::uint64_t ms = std::strtoull(argv[1], nullptr, 10);
    const std::string folder = argv[2];
    const int every = argc > 3 ? std::atoi(argv[3]) : 10;
    const bool video = argc > 4 && std::string(argv[4]) == "video";
    std::ofstream starts;
    if (video) {
        std::filesystem::create_directories(folder + "/video");
        starts.open(folder + "/video/frames.txt");
    }
    std::uint64_t frame_start = 0;

    auto m = std::make_unique<Vns4>();
    disconnect(*m);
    Emulator emu(MS0515_FIRMWARE_DIR "/ms0515-romb.rom");
    DramWatch dram;
    Lockstep lockstep;


    Frame frame;
    int frames = 0;
    std::uint64_t fetches = 0, matched = 0;
    bool diverged = false, started = false, ras_was = true, ksi_was = true, ssi_was = true, ts15_was = false;
    const auto wall = std::chrono::steady_clock::now();
    const std::uint64_t ticks = ms * 1000 * kMasterMhz;
    for (std::uint64_t t = 0; t < ticks; ++t) {
        power(*m, t);
        m->clk = 0;
        m->eval();
        m->clk = 1;
        m->eval();
        dram.see(*m);

        if (m->n_START_H) started = true;
        if (started && !m->n_START_H && !m->n_PVAS_L && ras_was && m->n_REZH0_H) {
            const std::uint16_t at = mk(*m);
            ++fetches;
            if (!diverged) {
                switch (lockstep.see(at, emu)) {
                case Lockstep::Same: ++matched; break;
                case Lockstep::Waiting: break;
                case Lockstep::Apart:
                    diverged = true;
                    std::printf("fetch %llu: the board at %06o, the emulator at %06o\n",
                                static_cast<unsigned long long>(fetches), at, lockstep.want);
                    break;
                }
            }
        }
        ras_was = m->n_PVAS_L != 0;

        if (!m->n_KSI_L && ksi_was) {                   // a frame ends
            if (video && !frame.lines.empty()) {
                char name[32];
                std::snprintf(name, sizeof name, "/video/%05d.bmp", frames);
                write_bmp(folder + name, frame);
                starts << frames << ' ' << frame_start << ' ' << t << '\n';
            }
            frame_start = t;
            if (frames % every == 0) {
                write_bmp(folder + "/frame" + std::to_string(frames) + ".bmp", frame);
                if (started) {
                    const ScreenCompare c = emu.screen(frame);
                    if (c.differ < 0)
                        std::printf("frame %d: too short to compare\n", frames);
                    else if (c.differ == 0)
                        std::printf("frame %d: as the emulator's screen (area at %d, %d)\n", frames, c.dx, c.dy);
                    else
                        std::printf("frame %d: %d samples differ (area at %d, %d); first at %d, %d: "
                                    "%x, the emulator's %x\n",
                                    frames, c.differ, c.dx, c.dy, c.first_x, c.first_y, c.got, c.want);
                }
            }
            frame.lines.clear();
            ++frames;
        }
        ksi_was = m->n_KSI_L != 0;
        if (!m->n_SSI_L && ssi_was) frame.lines.emplace_back();
        ssi_was = m->n_SSI_L != 0;
        if (m->n_TS15_H && !ts15_was && !frame.lines.empty() && frame.lines.back().size() < kLineSamples) {
            frame.lines.back().push_back(static_cast<std::uint8_t>(
                (m->n_SC_H & 1u) | (m->n_KC_H & 1u) << 1 | (m->n_ZC_H & 1u) << 2 | (m->nx_D69_9 & 1u) << 3));
        }
        ts15_was = m->n_TS15_H != 0;

        if (t % (10000ull * kMasterMhz) == 0 && t != 0) {
            const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall).count();
            std::printf("%llu ms: %llu fetches, %llu as the emulator's, %d frames (%.0f s)\n",
                        static_cast<unsigned long long>(t / (1000ull * kMasterMhz)),
                        static_cast<unsigned long long>(fetches), static_cast<unsigned long long>(matched),
                        frames, s);
            std::fflush(stdout);
        }
    }
    write_bmp(folder + "/last.bmp", frame);
    std::printf("done: %llu fetches, %llu as the emulator's, %d frames\n",
                static_cast<unsigned long long>(fetches), static_cast<unsigned long long>(matched), frames);
    std::printf("the emulator waited out %llu of the board's loops, %llu fetches in all\n",
                static_cast<unsigned long long>(lockstep.waits), static_cast<unsigned long long>(lockstep.waited));
    std::printf("the emulator's video memory: %zu of %d bytes not zero\n", emu.vram_used(), MEM_VRAM_SIZE);
    emu.print_vram_summary();
    const DramWatch::Stats& s = dram.stats();
    std::printf("RAM: %llu writes (%llu of 047777 words), %llu reads, %llu of them of written cells, "
                "%llu differ\n",
                static_cast<unsigned long long>(s.writes), static_cast<unsigned long long>(s.fills),
                static_cast<unsigned long long>(s.reads), static_cast<unsigned long long>(s.checked),
                static_cast<unsigned long long>(s.differ));
    std::printf("video: %llu reads, %llu of them 047777, %llu not zero\n",
                static_cast<unsigned long long>(s.video_reads), static_cast<unsigned long long>(s.video_fills),
                static_cast<unsigned long long>(s.video_nonzero));
    dram.print_cells();
    return 0;
}
