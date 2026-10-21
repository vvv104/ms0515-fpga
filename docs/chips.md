# Chips

Every chip type on НС4, its model in `rtl/chips/`, the part it is an
analogue of, and the document its function and its delay are read
from.  The delays and their sources are `netlist/delays.csv`
(`docs/timing.md`); the LSI chips have notes of their own.

The TTL models' function tables were written from the 74-series
functions and tested against them (`sim/chips/`); the documents below are
the references to check them against line by line, which is not yet done
for every chip.  The delays are taken from them already.

| Chip | Model | Analogue | Document |
|---|---|---|---|
| К555ЛА3 | `k555la3.sv` | 74LS00 | TI SN74LS00 SDLS025D sec 6.10, https://www.ti.com/lit/ds/symlink/sn74ls00.pdf |
| К555ЛА4 | `k555la4.sv` | 74LS10 | TI SN74LS10 SDLS035A p.4, https://www.ti.com/lit/ds/symlink/sn74ls10.pdf |
| К555ЛА2 | `k555la2.sv` | 74LS30 | TI SN74LS30 SDLS099 p.4, https://www.ti.com/lit/ds/symlink/sn74ls30.pdf |
| К555ЛА13 | `k555la13.sv` | 74LS38 | TI SN74LS38 SDLS105 p.4, https://www.ti.com/lit/ds/symlink/sn74ls38.pdf |
| К555ЛИ1 | `k555li1.sv` | 74LS08 | TI SN74LS08 SDLS033 p.4, https://www.ti.com/lit/ds/symlink/sn74ls08.pdf |
| К555ЛИ3 | `k555li3.sv` | 74LS11 | TI SN74LS11 SDLS131 p.3, https://www.ti.com/lit/ds/symlink/sn74ls11.pdf |
| К555ЛИ6 | `k555li6.sv` | 74LS21 | TI SN74LS21 SDLS139 p.3, https://www.ti.com/lit/ds/symlink/sn74ls21.pdf |
| К555ЛЛ1 | `k555ll1.sv` | 74LS32 | TI SN74LS32 p.4, https://www.ti.com/lit/ds/symlink/sn74ls32.pdf |
| КР531ЛЕ1 | `kr531le1.sv` | 74S02 | TI SN74S02 p.5, https://www.ti.com/lit/ds/symlink/sn74s02.pdf |
| К555ЛП5 | `k555lp5.sv` | 74LS86 | TI SN74LS86A SDLS124 p.4, https://www.ti.com/lit/ds/symlink/sn74ls86a.pdf |
| К555ЛН1 | `k555ln1.sv` | 74LS04 | TI SN74LS04 SDLS029C, https://www.ti.com/lit/ds/symlink/sn74ls04.pdf |
| К555ЛН2 | `k555ln2.sv` | 74LS05 | TI SN74LS05 SDLS030A, https://www.ti.com/lit/ds/symlink/sn74ls05.pdf |
| К155ЛН5 | `k155ln5.sv` | 7416 | TI SN7416 SDLS031A, https://www.ti.com/lit/ds/symlink/sn7416.pdf |
| К155ЛП9 | `k155lp9.sv` | 7407 | TI SN7407 SDLS032H sec 6.6, https://www.ti.com/lit/ds/symlink/sn7407.pdf |
| К555ТЛ2 | `k555tl2.sv` | 74LS14 | TI SN74LS14 SDLS049C sec 6.6, https://www.ti.com/lit/ds/symlink/sn74ls14.pdf |
| К555ЛР11 | `k555lr11.sv` | 74LS51 | TI SN74LS51 SDLS113 p.5, https://www.ti.com/lit/ds/symlink/sn74ls51.pdf |
| К555ИД7 | `k555id7.sv` | 74LS138 | TI SN74LS138 p.4, https://www.ti.com/lit/ds/symlink/sn74ls138.pdf |
| КР531ИД14 | `kr531id14.sv` | 74S139 | TI SN74S139A p.4, https://www.ti.com/lit/ds/symlink/sn74s139a.pdf |
| К555КП2 | `k555kp2.sv` | 74LS153 | TI SN74LS153 SDLS055A p.5, https://www.ti.com/lit/ds/symlink/sn74ls153.pdf |
| К555КП11 | `k555kp11.sv` | 74LS257 | TI SN74LS257B SDLS148 p.4, https://www.ti.com/lit/ds/symlink/sn74ls257b.pdf |
| К555КП15 | `k555kp15.sv` | 74LS251 | TI SN74LS251 SDLS085 p.6, https://www.ti.com/lit/ds/symlink/sn74ls251.pdf |
| К555ИВ3 | `k555iv3.sv` | 74LS147 | TI SN74LS147 SDLS053B p.10, https://www.ti.com/lit/ds/sdls053/sdls053.pdf |
| К555АП3 | `k555ap3.sv` | 74LS240 | TI SN74LS240 SDLS144D sec 6.7, https://www.ti.com/lit/ds/symlink/sn74ls240.pdf |
| К555АП5 | `k555ap5.sv` | 74LS244 | TI SN74LS244 SDLS144D sec 6.7, https://www.ti.com/lit/ds/symlink/sn74ls244.pdf |
| К555ТМ2 | `k555tm2.sv` | 74LS74 | TI SN74LS74A SDLS119 p.5, https://www.ti.com/lit/ds/symlink/sn74ls74a.pdf |
| КР531ТМ2 | `kr531tm2.sv` | 74S74 | TI SN74S74 SDLS119 p.6, https://www.ti.com/lit/ds/symlink/sn74s74.pdf |
| КР1533ТМ2 | `kr1533tm2.sv` | 74ALS74 | TI SN74ALS74A SDAS143C p.4, https://www.ti.com/lit/ds/symlink/sn74als74a.pdf |
| К555ТМ8 | `k555tm8.sv` | 74LS175 | TI SN74LS175 SDLS068A p.5, https://www.ti.com/lit/ds/symlink/sn74ls175.pdf |
| К555ТМ9 | `k555tm9.sv` | 74LS174 | TI SN74LS174 SDLS068A p.5, https://www.ti.com/lit/ds/symlink/sn74ls174.pdf |
| К555ИР22 | `k555ir22.sv` | 74LS373 | TI SN74LS373 SDLS165B p.7, https://www.ti.com/lit/ds/symlink/sn74ls373.pdf |
| К555ИР23 | `k555ir23.sv` | 74LS374 | TI SN74LS374 SDLS165B p.7, https://www.ti.com/lit/ds/symlink/sn74ls374.pdf |
| К555ИР16 | `k555ir16.sv` | 74LS295 | eandc К555ИР16 sheet (TI SN74LS295B not found), https://eandc.ru/pdf/mikroskhema/k555ir16.pdf |
| К555ИЕ10 | `k555ie10.sv` | 74LS161 | TI SN74LS161A SDLS060 p.15, https://www.ti.com/lit/ds/symlink/sn74ls161a.pdf |
| К555ИЕ19 | `k555ie19.sv` | 74LS393 | TI SN74LS393 SDLS107 p.8, https://www.ti.com/lit/ds/symlink/sn74ls393.pdf |
| К554СА3А | `k554sa3.sv` | LM311 | TI LM311 SLCS007K sec 6.7, https://www.ti.com/lit/ds/symlink/lm311.pdf |
| КР559ИП11 | `kr559ip11.sv` | Am26LS32 | TI AM26LS32A SLLS115G sec 6.6, https://www.ti.com/lit/ds/symlink/am26ls32a.pdf |
| КР1102АП15 | `kr1102ap15.sv` | uA9636 | TI uA9636AC SLLS110B p.3, https://www.ti.com/lit/ds/symlink/ua9636a.pdf |
| КР556РТ12 | `kr556rt12.sv` | 82S136 | Signetics 1977 Bipolar & MOS Memory p.123, https://xdevs.com/doc/eBooks/1977_Signetics_Bipolar_and_MOS_Memory.pdf |
| К573РФ4Б | `k573rf4.sv` | 2764 | НПП Восток К573РФ4 6КО.348.422-04ТУ p.2 (tACC tOE), https://eandc.ru/pdf/mikroskhema/k573rf4.pdf; ST M2764A-4 Table 7B p.5 (tDF), https://archive.org/download/2764_EPROM/2764_EPROM.pdf |
| КР565РУ5Г | `kr565ru5.sv` | 4164 | eandc К565РУ5 sheet p.2, https://eandc.ru/pdf/mikroskhema/k565ru5.pdf |
| КР1807ВМ1 | `kr1807vm1.sv` | DEC T-11 (DC310) | T-11 Engineering Specification |
| КР580ВИ53 | `kr580vi53.sv` | Intel 8253 | `docs/vi53.md` |
| КР580ВВ55А | `kr580vv55.sv` | Intel 8255A | `docs/vv55.md` |
| КР580ВВ51А | `kr580vv51.sv` | Intel 8251A | `docs/vv51.md` |
| КР1818ВГ93 | `kr1818vg93.sv` | WD FD1793 | `docs/vg93.md` |

Not chips but models the generator places:

| What | Model | From |
|---|---|---|
| a crystal and its oscillator (BQ1, BQ2) | `crystal.sv` | the rating in `netlist/parts.csv` |
| chips' propagation delays | `delay_bank.sv` | `netlist/delays.csv` |
