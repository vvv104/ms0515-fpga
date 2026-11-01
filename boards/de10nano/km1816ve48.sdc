# КМ1816ВЕ48 (the МС 7004's 8035, rtl/chips/km1816ve48.sv): every register
# of it but xtal_was changes only on an edge of its crystal, seen on the
# master tick as `half` - and the edges of the 4.608 MHz crystal come 13 or
# 14 master ticks apart (crystal_frac.sv).  So a path from one of those
# registers to another has two ticks and more; xtal_was, which makes `half`,
# changes every tick and keeps one.
set ve48 {*|km1816ve48:*|}
set acting [remove_from_collection [get_registers ${ve48}*] [get_registers ${ve48}xtal_was]]
set_multicycle_path -setup 2 -from $acting -to $acting
set_multicycle_path -hold 1 -from $acting -to $acting
