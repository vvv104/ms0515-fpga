// board_model: the generated board the bench's headers and run_board drive -
// НС4 alone (rtl/board/ns4.sv, Vns4), or, with MS0515_BOARD_EXT defined,
// НС4 with the memory expansion module on XS1 (rtl/board/ns4_ext.sv,
// Vns4_ext; plan 6a).  Both have the same ports; their insides differ in
// the top module's name alone, which MS0515_BOARD_CELLS puts on a path.
#pragma once

#ifdef MS0515_BOARD_EXT
#include "Vns4_ext.h"
#include "Vns4_ext___024root.h"
using Vboard = Vns4_ext;
#define MS0515_BOARD_CELLS(m, chip) ((m).rootp->ns4_ext__DOT__##chip##__DOT__cells)
#else
#include "Vns4.h"
#include "Vns4___024root.h"
using Vboard = Vns4;
#define MS0515_BOARD_CELLS(m, chip) ((m).rootp->ns4__DOT__##chip##__DOT__cells)
#endif
