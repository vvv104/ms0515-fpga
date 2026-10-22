# Firmware

The contents of the programmed chips on НС4, as the board's generator
(`tools/board.py`, through `netlist/firmware.csv`) builds them into the
chips' modules.  Each image is a dump of a real chip.

| File | Chip | What | md5 | Source |
|---|---|---|---|---|
| `d13_kdrv.r12` | D13, КР556РТ12 | frame (КДРВ) timing PROM, 1024 x 4: one word a byte, in its low four bits, bit k on output Ok | e8db8834e5d94bfab564af4596908b40 | read by Radon17 from a damaged НС4 (earlier also by Xolod); zx-pk.ru thread 15146 post #274, `MS0515_RT12.zip` |
| `d21_strv.r12` | D21, КР556РТ12 | line (СТРВ) timing PROM, the same format | 5a9f46244ebffc09fce9bb0704002aed | the same |
| `ms0515-romb.rom` | D65 (even bytes), D77 (odd bytes), К573РФ4 | ROM-B, 160000-177777 as 16-bit little-endian words; chip address = CPU address bits 13..1 | 3d60d719ca650b3fab27a0daf0a4d317 | the owner's machine, dumped in 2015 (`docs/findings.md`); the emulator's `src/assets/rom/ms0515-romb.rom` |
| `ms0515-roma.rom` | D65 and D77, put in the sockets by the bench (`rom=` of `run_board`, `sim/board/rom_socket.h`); the board is built with ROM-B | ROM-A, the same layout: the ROM with the cassette loader, the one OSA needs | b92cda2d83117ebbba2340acfadbf130 | KokaF77's machine, chips labelled «7004L»/«7004H», dumped in 2012 (zx-pk.ru thread 15146 p14 #134; `docs/findings.md`); the emulator's `src/assets/rom/ms0515-roma.rom` |
