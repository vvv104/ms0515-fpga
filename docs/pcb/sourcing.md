# НС4 anew: where to get the board and its parts

What was found on 2026-11-12 for a builder in Russia: fabs that can make
`kicad/ns4.kicad_pcb` (`docs/pcb/fab.md`) and shops for the parts of
`docs/pcb/bom.md`.  Prices are a snapshot; ChipDip's are its own and its
partners' listings (most deliver in 5-14 days), read off its search on that
day.  Nothing here has been ordered.

## The board

8 layers, 290.1 x 230.1 mm, 1.6 mm FR-4, track 0.19 mm, gap 0.2 mm, drill
0.5 mm: about 6.7 dm2 a board, 33 dm2 for five.

| Fab | Fits | Five boards | Ordering as a private person |
|---|---|---|---|
| PCB technology (pcbtech.ru) | yes: up to 24 layers, 500 x 800 mm, 0.1 mm track/gap | about 45,000 RUB before delivery and VAT - our reckoning from its published 8-layer rate (25,500 RUB set-up + 600 RUB/dm2), not its quote | not stated (the site is for companies); ask pcb@pcbtec.ru, 8 800 333-97-22; 3-4 weeks |
| Rezonit (rezonit.ru) | likely: FR-4 up to 350 x 490 mm over 0.8 mm, 18/35 um up to 24 layers | no 8-layer price found | not stated; ask pcb@rezonit.ru, 8 800 777-80-80; users report invoices 10-20 % over the calculator |
| PCB Russia (pcbrussia.ru) | unconfirmed: claims 1-64 layers, the calculator stops at 6 | no price; ask | yes: an individual's name on the form, card payment; 17-27 working days |
| NextPCB (China) | yes: 500 x 400 mm, 3/3 mil, 0.15 mm drill | no price found | its page says it delivers to Russia; payment through AliExpress; 3-4 weeks and more reported |
| JLCPCB (China) | yes: 656 x 586 mm (one board up to 400 x 500 mm delivered), 3.5/3.5 mil; choose 1 oz inner copper (0.5 oz is its default) | no price found for this size | not directly since 2022 by most reports; through a forwarder (below) |
| PCBWay (China) | yes: 560 x 1150 mm, 4/4 mil | no price found | no direct route to Russia found |

Other Russian fabs claiming multilayer boards, not checked further: ELECTROconnect,
Microlit (microlit.ru, up to 32 layers), Telerem (telerem.ru, 4-40 layers),
Mplata (mplata.ru, 8 layers in rush).

Forwarders that order from JLCPCB for rubles (a Habr article of June 2025):
PCBSolutions (t.me/pcbsolutions, 16 % markup, air 1,770 RUB/kg, ground
1,180 RUB/kg), "JLCPCB (LCSC) by Antony" (Telegram, 13 %), PCBChina
(pcbchina.ru, 19.3 % plus insurance; called expensive).  The markup is on the
whole order; Russia's duty-free limit is 200 EUR, which five eight-layer
boards pass.

The way to a real price: send the gerbers (`build/fab/ns4/ns4-gerbers.zip`)
to PCB technology, Rezonit and PCB Russia for a quote.  At this size a Russian
fab is likely no dearer than China plus a forwarder and customs.

## The parts

ChipDip (chipdip.ru) has nearly all of them.

| Part | Qty | Price each, RUB | Note |
|---|---|---|---|
| КР1818ВГ93 | 1 | ~2,900 | tens in stock |
| КР565РУ5Г | 16 | 310-550 | hundreds |
| КР556РТ12 | 2 | 52-180 | must be programmed (below) |
| К573РФ4А | 2 | 2,100-4,700 | РФ4Б not found; a 27C64 (the 2764's pins) is 180-420 |
| КР580ВИ53 | 1 | ~370 | |
| КР580ВВ51А | 2 | ~190 | |
| КР580ВВ55А | 2 | ~420 | uPD8255AC-2 ~140 |
| К1102АП15 | 2 | 20-160 | |
| КР559ИП11 | 1 | 65-110 | |
| КР531ИД14, КР531ЛЕ1, КР531ТМ2 | 1 each | 110-310 | |
| К155ЛП9, К155ЛН5 | 1 each | 20-450 | |
| КР1533ТМ2 | 1 | 560-2,100 | the dear one of the logic |
| К555 series | about 70 | mostly 40-500 | every type found (ИР16, ИВ3, ЛР11, КП15, ИР23 ... checked) |
| К554СА3А | 1 | ~850 | the 14-pin package: an LM311P (DIP-8) does not fit the holes |
| НР1-11А 1 kOhm, 10 kOhm | 1 each | - | not found; Bourns 4116R-2-102, 4116R-2-103 (15 resistors bussed to pin 16) 440-960 |
| СП5-16ВА 330 Ohm | 4 | 230-500 | the original trimmer, in stock |
| К53-14 33 uF 16 V | 4 | ~370 | |
| С2-23 23.7 kOhm 1 % | 3 | ~16 | |
| К10-17 0.22 uF and the other К10-17 | ~85 | 9-80 | |
| ОНП-ЖГ-79-9, -15, -25 | 1 each | 61-510 | the originals; the -9 and -25 listings sell to companies only |
| DIN 41612 C, 96 sockets, 2.54 mm | 1 | ~900 | sold to companies only there; СНП59-96 (420-600) has the 2.5 mm pitch and does not fit the footprint |
| ОНЦ-КГ-4-5/16-Р | 1 | ~230 | to companies only; XS2 may be left off |
| Crystals 8 MHz, 15 MHz, HC-49/U | 1 each | 22-48 | BQ1's holes are the РК 353's, 12.7 mm apart: an HC-49/U (4.88 mm) needs its leads lengthened |
| КД522Б, КС133А, АЛ307БМ | 13, 1, 3 | 25-100 | |
| ВДМ1-10 (SWD1-10) | 1 | 26-77 | |
| DIP sockets 28, 40 | 2, 2 | 30-160 | |
| IDC box headers 2x20, 2x17, pin headers | | 12-40 | |

All but the three below: roughly 40,000-60,000 RUB.

## The hard ones

- **КР1807ВМ1 (T-11)**: no shop.  Avito, eBay (as DEC DC310), the zx-pk.ru
  market; none of them could be read on 2026-11-12.
- **КТ676АС, КТ677АС**: no listing anywhere.  Э3 sheet 4 settles what they
  are: VT1 (КТ677АС) four NPN in common base (bases on two diodes' drop,
  collectors to +5 V through the trimmers), VT2 (КТ676АС) four PNP (emitters
  to +5 V through 24 Ohm, collectors into the 75 Ohm outputs).  Both have the
  pins C1 B1 E1 on 1-3, E2 B2 C2 on 5-7, C3 B3 E3 on 8-10, E4 B4 C4 on 12-14,
  4 and 11 free - Central Semiconductor's MPQ2222A pinout (pins 3-7 and
  10-14 read off its sheet, 1, 2, 8, 9 by its symmetry).  VT1: MPQ2222A,
  ChipDip 2,180-2,660 RUB, 6-7 weeks.  VT2: a PNP quad in DIP-14 with those
  pins (MPQ2907 kind) was not found on sale - ChipDip has only the SOIC-16
  MMPQ2907A; four 2N2907 (or КТ3107) on a small DIP-14 adapter do.
- **КР556РТ12**: a fusible-link PROM, programmed once.  The TL866 kind of
  programmer (about 7,000 RUB, it does the 27C64) does not do bipolar PROMs;
  it takes a programmer that does, or someone who has one.  The contents:
  `firmware/d13_kdrv.r12`, `firmware/d21_strv.r12`.

## The doubtful parts against their sheets

ChipDip's documents, kept in `ms0515_data/docs/chipdip/` (chips) and
`ms0515_data/docs/chipdip-parts/` (the rest):

- **СП5-16ВА** (DOC059521313): a round body 11 mm across, 9.7 mm high, three
  pins on three corners of a 3 x 3 mm square.  The 1991 holes of all four
  (R27, R31, R40, R50) stand 2.54 x 3.17 mm, so the sheet's 3 mm is either
  rounded or the board took the pins bent; to be tried with a part in hand
  before ordering.  Which pin is the wiper the sheet does not say; on НС4
  the wiper and one end are tied to +5 V.
- **К53-14** (DOC016051638): aluminium oxide-semiconductor, not tantalum;
  33 uF 16 V is 7.2 x 16 mm, which the 25.4 mm holes take.  Any axial 33 uF
  16 V does.
- **К554СА3А**: Э3 uses pins 3 (IN+), 4 (IN-), 9 (OUT) of a 14-pin package;
  an 8-pin LM311P has other pins and does not go in.  The 14-pin К554СА3А
  is on sale (above).
- No sheet found for ОНП-ЖГ-79, ОНЦ-КГ-4-5/16-Р, СНП59-96, РК353, РК169,
  НР1-11; their footprints stay as the 1991 holes have them.

## The chips' delays against Soviet sheets

The Soviet sheets ChipDip carries give maximum delays only, and they are
mostly TI's maximums as `netlist/delays-max.csv` has them: К555ИД7 18/32,
20/41, 27/39, 26/38 ns as the SN74LS138; К555КП2 15/26, 24/32, 29/38 ns as
the SN74LS153; КР1533ТМ2 CLK->Q 16/18 ns as the SN74ALS74A.  КР1533ИЕ10
differs: CLK->Q 15/17 ns (SN74ALS161B 15/20), CLK->carry 23/26 ns (20),
CLR->Q 24 and ENT->carry 13 as TI's.  Its CLK->Q at the Soviet maximum
rounds to the same ticks as now (2 at max, 1 at typical), so
`netlist/delays.csv` and `delays-max.csv` stand as they are.

## Sources

- JLCPCB capabilities: https://jlcpcb.com/capabilities/Capabilities
- JLCPCB customs: https://jlcpcb.com/help/article/Customs,-Duties-and-Taxes
- NextPCB for Russia: https://www.nextpcb.com/blog/pcb-for-russia
- Fabs compared: https://www.nextpcb.com/blog/pcb-capabilities-comparison-jlcpcb-vs-pcbway-vs-nextpcb
- PCB technology: https://www.pcbtech.ru/izgotovlenie-pechatnykh-plat
- Rezonit: https://www.rezonit.ru/pcb/mnogosloynye-pechatnye-platy/
- PCB Russia: https://pcbrussia.ru/
- Forwarders: https://habr.com/en/articles/925082
- Parts: ChipDip's search, https://www.chipdip.ru/
