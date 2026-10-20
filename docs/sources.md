# Sources

Everything the board is read from.  Local copies live outside the
repository under `$MS0515_DATA` (default `../ms0515_data`); paths below are
relative to it.  Status: **have** = local copy, **read** = transcribed or
traced into this repository, **want** = known to exist, not yet obtained.

## The system module НС4 (the board itself)

| What | Where | Status |
|---|---|---|
| `3.858.420 Э3` - НС4 schematic, 4 sheets | `scans/mc0515-ed.rar` (djvu, no text layer), pages 19-22; rendered in `scans/mc0515-ed-djvu/e3_19..22.png` | have |
| `3.858.420 ПЭ3` - НС4 element list | same djvu, pages 23-26 | have |
| `3.858.420 СБ` - НС4 assembly drawing | same djvu, page 27 | have |
| `3.858.420 ТО` - НС4 technical description (block diagrams, timing diagrams, registers) | `scans/hc4-to.rar`; text in `docs/NS4-techdesc.md` | have, text read |
| НС4 rev. 3А bare board, both sides, 6630x5233 | `scans/boards/NS4_red3A_1.tif`, `NS4_red3A_8.tif` (from radon.su) | have |
| НС5 rev. 01 - НС4 re-routed for the УКНЦ case, both sides | `scans/boards/NS5_1.png`, `NS5_8.png` (from radon.su) | have |
| НС4 traced layer by layer by **Mick** (from Radon17's scan): signal layers 1, 2, 7, 8 and all together, 4488x3543 - layers 3-6 not traced | `scans/boards/radon17-layers/NS4-{1,2,7,8,all}.gif` - zx-pk.ru thread 15146 post #271 (2023-09-15), images on ibb.co | have |
| The same board translated into P-CAD by Mick (picture only) | `scans/boards/radon17-layers/NS4-pcad.png`, same post | have |
| НС5 rev. 01 traced by Mick: layers 1, 2, 7, 8 and also 1+3, 6+7 | `scans/boards/mick-ns5-layers/` - thread 15146 post #266 (2023-09-10) | have |
| НС4 **rev. 3** (not 3А), both sides, ~396 MB each | `scans/boards/ns4-red3/` - Radon17's Yandex Disk folder «Э85-сканы/НС4» (https://disk.yandex.ru/d/FjOsY6VLrxZHiQ), linked in thread 15146 post #267 | have |
| **Mick's P-CAD databases** of НС4 and НС5 (the netlists of the copper) and the inner layers 3-6 of НС4 | - | want - ask Mick (micklab.ru) |
| К556РТ12 PROMs D13 (КДРВ) and D21 (СТРВ) on the video output, read by Radon17 from a damaged НС4 (also read earlier by Xolod) | `MS0515_RT12.zip` - zx-pk.ru 15146 post #274 (2024-10-20) | have |
| Photos of assembled machines and boards | `photos/`, `docs/codemaster-processor/` | have |
| The other НС4 revisions (four are known) | - | want |
| CodeMaster's plant archive (`retro.codemaster.ru/Processor/`, mostly lost) | inventory in `docs/codemaster-processor/INDEX.md` | want - ask CodeMaster |

## The machine around the board

| What | Where | Status |
|---|---|---|
| `2.791.064 ТО` - МС 0515 technical description | `scans/mc0515-to.rar`; pages in `scans/mc0515-to-djvu/` | have |
| `2.791.064 ЭД` - the operating documents, incl. `Э6` general diagram, `3.559.024 Э4` interconnection | `scans/mc0515-ed.rar` | have |
| `3.559.025 Э3` - the sound unit | not found yet | want |
| Cables `4.854.127..131 Э3` | not found yet | want |
| МС 7004 keyboard: schematic, ТО, firmware | `scans/ps-kb.rar`, `kbms7004.djvu`; firmware in the emulator's `src/assets` | have |
| Floppy drive (НГМД) documentation | `scans/ngmd-nmd.rar` | have |
| Memory expansion module («Модуль расширения ЗУ и интерфейсов»: 512 KB of КР565РУ7Г as a RAM disk + two RS-232C) - its description, element list (D1..D35, R1..R27, G1 5.0688 MHz, XP1 to the board's XS1) and **connection tables** (`l1..l6_1.doc.txt`, cp1251) - a netlist in table form, no schematic needed | `Модуль расширения запоминающего устройства 0515.zip` (anasana, zx-pk 15146 p16 #154), `docs/ext/` | have |
| Photos of the owner's RAM-disk board | `photos/known-machines/0_d4979_65e0bd2a_XXXL.jpg`, `0_d497a_ec95e9c1_XXXL.jpg` | have |
| ROM-A / ROM-B images and listings | `docs/ms0515-roma.*`, `docs/ms0515-romb.*` | have |
| Factory documentation inventory | ms0515-software `refs/docs-inventory.md` | read |

## Chips

| What | Where | Status |
|---|---|---|
| T-11 Engineering Specification Rev E (Mar 1982) - microcycles, pin timing | `scans/t11/T11_Engineering_Specification_Rev_E_Mar82.pdf` (+ `.txt`) | have |
| T-11 User's Guide | `scans/t11/T11_UsersMan.pdf` (+ `.txt`) | have |
| КР1818ВГ93 (WD1793), КР580ВИ53 (8253), КР580ВВ51 (8251), КР580ВВ55 (8255), К565РУ*, К573РФ* datasheets | - | want |
| К555 / К1533 / К531 function tables (Soviet reference books, TI 74LS data) | - | want |

## Forum and web

| What | Where |
|---|---|
| zx-pk.ru thread 15146 "МС-0515" - the machine | https://zx-pk.ru/threads/15146-ms-0515.html |
| zx-pk.ru thread 24333 - the history of the «Процессор» plant (НС4 = УБПК-16, revisions) | https://zx-pk.ru/threads/24333-istoriya-zavoda-quot-protsessor-quot-g-voronezh/ |
| zx-pk.ru thread 36741 - the emulator's own thread | https://zx-pk.ru/threads/36741-eshchjo-odin-emulyator-ms-0515.html |
| radon.su board scans of the plant (НС1, НС3, НС4, НС5, НС8, ...) | https://radon.su/files/scan/brd/processor/ |
| tis.kz scanned documentation (the RARs above came from there) | http://www.tis.kz/docs/ |
| bitsavers - DEC T-11 documents | http://www.bitsavers.org/pdf/dec/ |

The forum serves attachments only with a browser's User-Agent and Referer.

## Already known from this project family

- The emulator's `docs/hardware/` - the machine as the emulator models it,
  verified against original software.
- Interrupt logic read from Э3 sheet 2 (page 19): requests latched in D89
  К555ИР23 on ПВАК, one code chosen by D96 К555ИВ3; outputs A0→CP0,
  A1→CP1, A2→CP3, A3→CP2.  The schematic's priority order hung OSA's ^Q
  in the emulator; the emulator moved MON first.  Which build the
  schematic shows is unknown - see `docs/findings.md`.
- The МС 1702 repository (`ms0515-mc1702`) read a board out of Э3 scans
  into a netlist and a generated KiCad schematic; its method and its wire
  tracer are the starting point for `netlist/` here.
