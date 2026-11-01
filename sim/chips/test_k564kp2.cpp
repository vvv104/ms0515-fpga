// 564КП2 (CD4051): one of eight channels X0..X7 onto X by C B A, none while
// INH is high - in logic levels, the line picked passes.
#include "chip.h"

#include "Vk564kp2.h"

TEST_CASE("564КП2: the channel C B A picks passes to X, none under INH")
{
    using V = Vk564kp2;
    Chip<V> chip({PIN(1), PIN(2), PIN(3), PIN(4), PIN(5), PIN(6), PIN(9), PIN(10), PIN(11), PIN(12),
                  PIN(13), PIN(14), PIN(15)});
    const int channel_pin[8] = {13, 14, 15, 12, 1, 5, 2, 4};
    for (int inh = 0; inh < 2; ++inh)
        for (int sel = 0; sel < 8; ++sel)
            for (int pattern : {0x00, 0xFF, 0x55, 0xAA, 0x0F, 0x96}) {
                chip.set(6, inh != 0);
                chip.set(11, (sel & 1) != 0);
                chip.set(10, (sel & 2) != 0);
                chip.set(9, (sel & 4) != 0);
                for (int c = 0; c < 8; ++c)
                    chip.set(channel_pin[c], ((pattern >> c) & 1) != 0);
                chip.eval();
                const bool want = !inh && ((pattern >> sel) & 1) != 0;
                CHECK(chip.get(3) == want);
            }
}
