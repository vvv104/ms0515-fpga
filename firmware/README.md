# Firmware

The contents of the programmed chips on НС4, as the board's generator
(`tools/board.py`, through `netlist/firmware.csv`) builds them into the
chips' modules.  Each image is a dump of a real chip.

| File | Chip | What | md5 | Source |
|---|---|---|---|---|
| `d13_kdrv.r12` | D13, КР556РТ12 | frame (КДРВ) timing PROM, 1024 x 4: one word a byte, in its low four bits, bit k on output Ok | e8db8834e5d94bfab564af4596908b40 | read by Radon17 from a damaged НС4 (earlier also by Xolod); zx-pk.ru thread 15146 post #274, `MS0515_RT12.zip` |
| `d21_strv.r12` | D21, КР556РТ12 | line (СТРВ) timing PROM, the same format | 5a9f46244ebffc09fce9bb0704002aed | the same |
