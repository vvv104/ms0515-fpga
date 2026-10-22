// run_board: the generated НС4 board from power-on for a stretch of machine
// time - the processor's fetches checked against the emulator's, and the
// picture taken off the video DAC's digital inputs, frame by frame.
//
//   run_board <machine ms> <folder for the frames> [every Nth frame] [video] [seed=N]
//             [disk=IMAGE] [layout=omega|vvv|fdz] [keyboard] [save=FILE@MS] [load=FILE]
//             [rom=IMAGE] [watch=LO-HI] [spot=ROW:COLUMN]
//
// Every Nth frame is saved and compared with the emulator's screen; with
// `video`, every frame is saved as well, into <folder>/video, with the
// master tick each began at in video/frames.txt - tools/frames_to_video.py
// makes them a film in the machine's own time.  seed=N powers the board up
// in Verilator's random state from seed N (0, the default: all zero).
// disk=IMAGE puts the image in drive 0, the МС 5305 on XP4 (sim/board/
// drive.h), its tracks laid out as the formatter `layout` writes them;
// `keyboard` puts the stub on XS2 that answers ROM-B's ID request.
// save=FILE@MS (as many as wanted) writes a snapshot of the board and the bench at machine
// time MS; load=FILE goes on from one, to machine time <machine ms> (no
// lockstep then: the emulator starts at power-on).  rom=IMAGE puts another
// ROM pair in the sockets (ROM-A: firmware/ms0515-roma.rom), the emulator
// too; a snapshot carries the ROM it was taken with.  watch= and spot=:
// docs/bench.md.  The last line,
// "summary: key=value ...", is for tools/run_many.py.
//
// A frame is the lines between two frame syncs (КСИ L), a line the samples
// at each ТС15 H rise from the line sync (ССИ L): the blue, red and green
// levels (СЦ, КЦ, ЗЦ H) and the brightness line (D69.9).  Frames are BMP
// files, the whole line with its blanking (960 x the frame's lines).
#include <verilated.h>


#include "Vns4.h"
#include "power.h"
#include "keyboard_stub.h"
#include "xp4.h"
#include "dram_watch.h"
#include "screen_compare.h"
#include "snapshot.h"
#include "snapshot_file.h"
#include "rom_socket.h"

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
#include <utility>
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

struct Options {
    std::uint64_t ms = 0;
    std::string folder;
    int every = 10;
    bool video = false;
    int seed = 0;
    std::string disk;                   // an image in drive 0, the МС 5305 (none: no disk)
    std::string layout = "omega";       // the formatter its tracks are laid out as
    bool keyboard = false;              // the keyboard stub on XS2 (sim/board/keyboard_stub.h)
    std::vector<std::pair<std::uint64_t, std::string>> saves;  // snapshots: (machine ms, file)
    std::string load;                   // a snapshot the run goes on from
    std::string rom;                    // another ROM image in the sockets (sim/board/rom_socket.h)
    std::vector<std::pair<unsigned, unsigned>> watch;  // writes logged: address ranges, octal
    std::vector<std::size_t> spots;     // DRAM cells (row:column, octal) whose video reads are placed

    drive::Layout track_layout() const
    {
        return layout == "vvv" ? drive::vvv() : layout == "fdz" ? drive::fdz() : drive::omega();
    }

    // false on a bad command line
    bool parse(int argc, char** argv)
    {
        if (argc < 3)
            return false;
        ms = std::strtoull(argv[1], nullptr, 10);
        folder = argv[2];
        every = argc > 3 ? std::atoi(argv[3]) : 10;
        for (int a = 4; a < argc; ++a) {
            const std::string arg = argv[a];
            if (arg == "video")
                video = true;
            else if (arg.rfind("seed=", 0) == 0)
                seed = std::atoi(arg.c_str() + 5);
            else if (arg.rfind("disk=", 0) == 0)
                disk = arg.substr(5);
            else if (arg.rfind("layout=", 0) == 0)
                layout = arg.substr(7);
            else if (arg == "keyboard")
                keyboard = true;
            else if (arg.rfind("save=", 0) == 0 && arg.find('@') != std::string::npos)
                saves.emplace_back(std::strtoull(arg.c_str() + arg.find('@') + 1, nullptr, 10),
                                   arg.substr(5, arg.find('@') - 5));
            else if (arg.rfind("load=", 0) == 0)
                load = arg.substr(5);
            else if (arg.rfind("rom=", 0) == 0)
                rom = arg.substr(4);
            else if (arg.rfind("watch=", 0) == 0 && arg.find('-') != std::string::npos)
                watch.emplace_back(static_cast<unsigned>(std::strtoul(arg.c_str() + 6, nullptr, 8)),
                                   static_cast<unsigned>(std::strtoul(arg.c_str() + arg.find('-') + 1, nullptr, 8)));
            else if (arg.rfind("spot=", 0) == 0 && arg.find(':') != std::string::npos)
                spots.push_back(std::strtoul(arg.c_str() + 5, nullptr, 8) << 8 |
                                std::strtoul(arg.c_str() + arg.find(':') + 1, nullptr, 8));
        }
        return every > 0;
    }
};

// The processor's writes to the address ranges asked for (watch=LO-HI,
// octal): the address off АД as the row strobe falls, the bytes written
// (ПМЛБ / ПСТБ low), the data off МК as the processor's column strobe rises.
class Writes {
public:
    std::vector<std::pair<unsigned, unsigned>> ranges;

    void see(const Vns4& m, std::uint64_t t, const DramWatch& dram)
    {
        if (ranges.empty()) return;
        const bool ras = m.n_PVAS_L != 0;
        if (!ras && ras_was_) {
            at_ = ad(m);
            lo_ = hi_ = false;
            ram_writes_ = dram.stats().writes;
        }
        if (!ras) {
            lo_ = lo_ || !m.n_PMLB_L;
            hi_ = hi_ || !m.n_PSTB_L;
            if (m.n_PVAK_L && !cas_was_ && (lo_ || hi_)) data_ = mk(m);
        }
        if (ras && !ras_was_ && (lo_ || hi_) && wanted(at_)) {
            std::printf("%.6f s: write %06o %s %06o", static_cast<double>(t) / (1e6 * kMasterMhz), at_,
                        lo_ && hi_ ? "word" : lo_ ? "low " : "high", data_);
            if (dram.stats().writes != ram_writes_)
                std::printf(" cell %03zo:%03zo", dram.last_write() >> 8, dram.last_write() & 0xFF);
            std::printf("\n");
        }
        ras_was_ = ras;
        cas_was_ = m.n_PVAK_L != 0;
    }

private:
    // АД00..АД15 as a word
    static unsigned ad(const Vns4& m)
    {
        const CData b[16] = {m.n_AD00_H, m.n_AD01_H, m.n_AD02_H, m.n_AD03_H, m.n_AD04_H, m.n_AD05_H,
                             m.n_AD06_H, m.n_AD07_H, m.n_AD08_H, m.n_AD09_H, m.n_AD10_H, m.n_AD11_H,
                             m.n_AD12_H, m.n_AD13_H, m.n_AD14_H, m.n_AD15_H};
        unsigned v = 0;
        for (int i = 0; i < 16; ++i) v |= (b[i] & 1u) << i;
        return v;
    }

    bool wanted(unsigned a) const
    {
        for (const auto& [lo, hi] : ranges)
            if (a >= lo && a <= hi) return true;
        return false;
    }

    bool ras_was_ = true, cas_was_ = true, lo_ = false, hi_ = false;
    unsigned at_ = 0, data_ = 0;
    std::uint64_t ram_writes_ = 0;      // the DRAM's writes as the cycle began: a cell only if one more
};

// The processor's fetches (a row strobe of a fetch once СТАРТ is over)
// against the emulator's.  СТАРТ is the pulse once the supply is up: a
// random power-up state may show it high before, and the processor may run
// until the real pulse resets it.
class Fetches {
public:
    std::uint64_t fetches = 0, matched = 0;
    bool diverged = false;
    Lockstep lockstep;

    void see(const Vns4& m, Emulator& emu, std::uint64_t t)
    {
        if (m.n_START_H && t >= kSupplyRiseTicks) started_ = true;
        if (started_ && !m.n_START_H && !m.n_PVAS_L && ras_was_ && m.n_REZH0_H)
            fetch(mk(m), emu);
        ras_was_ = m.n_PVAS_L != 0;
    }
    bool started() const { return started_; }
    void resume() { started_ = true; }      // from a snapshot, taken after СТАРТ

private:
    void fetch(std::uint16_t at, Emulator& emu)
    {
        ++fetches;
        if (diverged)
            return;
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

    bool started_ = false, ras_was_ = true;
};

// The picture off the video DAC's inputs, frame by frame: every Nth saved
// and compared with the emulator's screen, every one saved for a film.
class Camera {
public:
    Frame frame;
    int frames = 0, last_differ = -1;

    explicit Camera(const Options& o) : o_(o)
    {
        if (o.video) {
            std::filesystem::create_directories(o.folder + "/video");
            starts_.open(o.folder + "/video/frames.txt");
        }
    }

    void see(const Vns4& m, std::uint64_t t, const Emulator& emu, bool started)
    {
        if (!m.n_KSI_L && ksi_was_)
            frame_ends(t, emu, started);
        ksi_was_ = m.n_KSI_L != 0;
        if (!m.n_SSI_L && ssi_was_) frame.lines.emplace_back();
        ssi_was_ = m.n_SSI_L != 0;
        if (m.n_TS15_H && !ts15_was_ && !frame.lines.empty() && frame.lines.back().size() < kLineSamples)
            frame.lines.back().push_back(static_cast<std::uint8_t>(
                (m.n_SC_H & 1u) | (m.n_KC_H & 1u) << 1 | (m.n_ZC_H & 1u) << 2 | (m.nx_D69_9 & 1u) << 3));
        ts15_was_ = m.n_TS15_H != 0;
    }

private:
    void frame_ends(std::uint64_t t, const Emulator& emu, bool started)
    {
        if (o_.video && !frame.lines.empty()) {
            char name[32];
            std::snprintf(name, sizeof name, "/video/%05d.bmp", frames);
            write_bmp(o_.folder + name, frame);
            starts_ << frames << ' ' << frame_start_ << ' ' << t << '\n';
        }
        frame_start_ = t;
        if (frames % o_.every == 0) {
            write_bmp(o_.folder + "/frame" + std::to_string(frames) + ".bmp", frame);
            if (started)
                compare(emu);
        }
        frame.lines.clear();
        ++frames;
    }

    void compare(const Emulator& emu)
    {
        const ScreenCompare c = emu.screen(frame);
        last_differ = c.differ;
        if (c.differ < 0)
            std::printf("frame %d: too short to compare\n", frames);
        else if (c.differ == 0)
            std::printf("frame %d: as the emulator's screen (area at %d, %d)\n", frames, c.dx, c.dy);
        else
            std::printf("frame %d: %d samples differ (area at %d, %d); first at %d, %d: %x, the emulator's %x\n",
                        frames, c.differ, c.dx, c.dy, c.first_x, c.first_y, c.got, c.want);
    }

    const Options& o_;
    std::ofstream starts_;
    std::uint64_t frame_start_ = 0;
    bool ksi_was_ = true, ssi_was_ = true, ts15_was_ = false;
};

// Where on the screen the video reads the DRAM cells asked for
// (spot=ROW:COLUMN, octal): the frame's line and sample of the first read
// of each cell in each frame.
class Spots {
public:
    explicit Spots(std::vector<std::size_t> cells) : cells_(std::move(cells)), seen_(cells_.size(), -1) {}

    void see(const DramWatch& dram, const Camera& camera, std::uint64_t t)
    {
        if (cells_.empty() || dram.stats().video_reads == reads_) return;
        reads_ = dram.stats().video_reads;
        const auto spot = std::find(cells_.begin(), cells_.end(), dram.last_video());
        if (spot == cells_.end() || camera.frame.lines.empty()) return;
        int& seen = seen_[static_cast<std::size_t>(spot - cells_.begin())];
        if (seen == camera.frames) return;
        seen = camera.frames;
        std::printf("%.6f s: video reads cell %03zo:%03zo at line %zu, sample %zu\n",
                    static_cast<double>(t) / (1e6 * kMasterMhz), *spot >> 8, *spot & 0xFF,
                    camera.frame.lines.size() - 1, camera.frame.lines.back().size());
    }

private:
    std::vector<std::size_t> cells_;
    std::vector<int> seen_;
    std::uint64_t reads_ = 0;
};

void report(const Options& o, const Fetches& f, const Camera& cam, const Emulator& emu, const DramWatch& dram)
{
    using ull = unsigned long long;
    std::printf("done: %llu fetches, %llu as the emulator's, %d frames\n", static_cast<ull>(f.fetches),
                static_cast<ull>(f.matched), cam.frames);
    std::printf("the emulator waited out %llu of the board's loops, %llu fetches in all\n",
                static_cast<ull>(f.lockstep.waits), static_cast<ull>(f.lockstep.waited));
    std::printf("the emulator's video memory: %zu of %d bytes not zero\n", emu.vram_used(), MEM_VRAM_SIZE);
    emu.print_vram_summary();
    const DramWatch::Stats& s = dram.stats();
    std::printf("RAM: %llu writes (%llu of 047777 words), %llu reads, %llu of them of written cells, %llu differ\n",
                static_cast<ull>(s.writes), static_cast<ull>(s.fills), static_cast<ull>(s.reads),
                static_cast<ull>(s.checked), static_cast<ull>(s.differ));
    std::printf("video: %llu reads, %llu of them 047777, %llu not zero\n", static_cast<ull>(s.video_reads),
                static_cast<ull>(s.video_fills), static_cast<ull>(s.video_nonzero));
    dram.print_cells();
    std::printf("summary: ms=%llu seed=%d fetches=%llu matched=%llu diverged=%d waits=%llu waited=%llu frames=%d "
                "last_differ=%d\n",
                static_cast<ull>(o.ms), o.seed, static_cast<ull>(f.fetches), static_cast<ull>(f.matched),
                f.diverged ? 1 : 0, static_cast<ull>(f.lockstep.waits), static_cast<ull>(f.lockstep.waited),
                cam.frames, cam.last_differ);
}

}  // namespace

// The parts of a run that a snapshot carries: the board, the bench's
// drive and keyboard, the tick it was taken at, the frames so far.
struct Parts {
    Vns4& m;
    std::unique_ptr<drive::Mc5305>& fdd;
    keyboard::Stub& kbd;
    int& frames;
};

void save_snapshot(const std::string& path, Parts p, std::uint64_t t)
{
    SnapshotSave os(path);
    os << p.m;
    SaveArchive<SnapshotSave> a{os};
    a(t);
    a(p.frames);
    a(p.fdd != nullptr);
    if (p.fdd) p.fdd->state(a);
    p.kbd.state(a);
    os.close();
    std::printf("%.3f s: snapshot saved to %s\n", static_cast<double>(t) / (1e6 * kMasterMhz), path.c_str());
}

// the tick the snapshot was taken at; the drive made if it had one
std::uint64_t load_snapshot(const std::string& path, Parts p, const Options& o)
{
    SnapshotRestore is(path);
    is >> p.m;
    LoadArchive<SnapshotRestore> a{is};
    std::uint64_t t = 0;
    a(t);
    a(p.frames);
    bool disk = false;
    a(disk);
    if (disk) {
        if (!p.fdd) p.fdd = std::make_unique<drive::Mc5305>(std::vector<std::uint8_t>{}, o.track_layout());
        p.fdd->state(a);
    }
    p.kbd.state(a);
    is.close();
    std::printf("%.3f s: the run goes on from %s; the emulator is not followed from a snapshot\n",
                static_cast<double>(t) / (1e6 * kMasterMhz), path.c_str());
    return t;
}

int main(int argc, char** argv)
{
    Options o;
    if (!o.parse(argc, argv)) {
        std::fprintf(stderr, "usage: run_board <machine ms> <folder> [every Nth frame] [video] [seed=N]\n");
        return 2;
    }
    // the power-up state, set before the model is made
    Verilated::randReset(o.seed == 0 ? 0 : 2);
    Verilated::randSeed(o.seed);
    auto m = std::make_unique<Vns4>();
    disconnect(*m);
    Emulator emu(o.rom.empty() ? MS0515_FIRMWARE_DIR "/ms0515-romb.rom" : o.rom);
    if (!o.rom.empty() && o.load.empty()) {
        std::ifstream f(o.rom, std::ios::binary);
        if (!plug_rom(*m, {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()})) {
            std::fprintf(stderr, "%s: not a 16 KB ROM image\n", o.rom.c_str());
            return 2;
        }
    }
    DramWatch dram;
    Fetches fetches;
    Writes writes;
    writes.ranges = o.watch;
    Camera camera(o);
    Spots spots(o.spots);
    keyboard::Stub kbd;
    std::size_t heard = 0;
    std::unique_ptr<drive::Mc5305> fdd;
    if (!o.disk.empty()) {
        std::ifstream f(o.disk, std::ios::binary);
        std::vector<std::uint8_t> image{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
        std::printf("drive 0: %s, %zu bytes, laid out as %s's\n", o.disk.c_str(), image.size(), o.layout.c_str());
        fdd = std::make_unique<drive::Mc5305>(std::move(image), o.track_layout());
    }

    const Parts parts{*m, fdd, kbd, camera.frames};
    std::uint64_t from = 0;
    if (!o.load.empty()) {
        from = load_snapshot(o.load, parts, o);
        fetches.diverged = true;                       // the emulator starts at power-on: no lockstep
        fetches.resume();
    }
    std::size_t next_save = 0;                          // saves in the order of their times
    std::vector<std::pair<std::uint64_t, std::string>> saves = o.saves;
    std::sort(saves.begin(), saves.end());

    const auto wall = std::chrono::steady_clock::now();
    const std::uint64_t ticks = o.ms * 1000 * kMasterMhz;
    for (std::uint64_t t = from; t < ticks; ++t) {
        while (next_save < saves.size() && t == saves[next_save].first * 1000 * kMasterMhz)
            save_snapshot(saves[next_save++].second, parts, t);
        power(*m, t);
        if (fdd)
            xp4_drive(*m, fdd->tick(t, xp4_lines(*m)));
        if (o.keyboard) {
            xs2_keyboard(*m, kbd, t);
            if (kbd.heard().size() != heard) {
                std::printf("%.3f s: the keyboard heard %03o\n", static_cast<double>(t) / (1e6 * kMasterMhz),
                            kbd.heard().back());
                heard = kbd.heard().size();
            }
        }
        m->clk = 0;
        m->eval();
        m->clk = 1;
        m->eval();
        dram.see(*m);
        fetches.see(*m, emu, t);
        writes.see(*m, t, dram);
        camera.see(*m, t, emu, fetches.started());
        spots.see(dram, camera, t);
        if (t % (10000ull * kMasterMhz) == 0 && t != 0) {
            const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall).count();
            std::printf("%llu ms: %llu fetches, %llu as the emulator's, %d frames (%.0f s)\n",
                        static_cast<unsigned long long>(t / (1000ull * kMasterMhz)),
                        static_cast<unsigned long long>(fetches.fetches),
                        static_cast<unsigned long long>(fetches.matched), camera.frames, s);
            std::fflush(stdout);
        }
    }
    write_bmp(o.folder + "/last.bmp", camera.frame);
    report(o, fetches, camera, emu, dram);
    return 0;
}
