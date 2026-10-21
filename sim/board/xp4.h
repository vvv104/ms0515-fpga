// xp4: the floppy drive on the board's connector XP4, by the cable
// 4.854.127 (ЭД, a straight 34-way ribbon to the МС 5305).  The board's
// lines are read off its nets as they stood after the last tick; the
// drive's open-collector outputs pull their pins low or let them go.
#pragma once

#include "drive.h"

#include <cstdint>

template <class V>
drive::Lines xp4_lines(const V& m)
{
    drive::Lines l;
    l.select0 = m.n_VN0_H != 0;         // pin 10
    l.select1 = m.n_VN1_H != 0;         // 12
    l.select2 = m.n_VN2_H != 0;         // 14
    l.select3 = m.n_VN3_H != 0;         // 6
    l.motor = m.n_DV_H != 0;            // 16, МВК L at the drive
    l.direction = m.n_NPR_L != 0;       // 18
    l.step = m.n_SHAG_L != 0;           // 20
    l.write_data = m.n_ZAP_L != 0;      // 22, ДЗП L
    l.write_gate = m.n_RZP_L != 0;      // 24
    l.side = m.n_VBP_H != 0;            // 32
    return l;
}

// the keyboard on XS2: the board's ВЫХКЛ L (pin 2) in, ВХКЛ L (pin 3)
// driven, its line's levels as the board's driver and receiver make them
template <class V, class Keyboard>
void xs2_keyboard(V& m, Keyboard& k, std::uint64_t t)
{
    m.x_XS2_p3 = k.tick(t, m.n_VYHKL_L != 0) ? 1 : 0;
    m.x_XS2_p3_oe = 1;
}

// an open collector: low pulls, high lets the pin go to the board's pull-up
template <class V>
void xp4_drive(V& m, const drive::Outputs& o)
{
    m.x_XP4_p8 = 0;                     // ИНД
    m.x_XP4_p8_oe = o.index ? 0 : 1;
    m.x_XP4_p26 = 0;                    // Д00
    m.x_XP4_p26_oe = o.track0 ? 0 : 1;
    m.x_XP4_p28 = 0;                    // ЗЗП
    m.x_XP4_p28_oe = o.protect ? 0 : 1;
    m.x_XP4_p30 = 0;                    // ДВС, the board's ЧТН
    m.x_XP4_p30_oe = o.data ? 0 : 1;
    m.x_XP4_p34 = 0;                    // НГТ, the board's ГТВ
    m.x_XP4_p34_oe = o.ready ? 0 : 1;
}
