# КР1807ВМ1's core acts on the bus's events two ticks after them, and the
# events come three ticks apart at the least (rtl/chips/kr1807vm1_core.sv;
# test_kr1807vm1_lockstep holds the gap).  So what the core acts on - its
# own state and the data and AI the bus latched - has held for two ticks
# when it acts: two ticks for its arithmetic.  The event pipeline, the
# mode word read under -BCLR, the power-up count and -BCLR itself change
# on other ticks and keep one.  For the board alone (synth.tcl) and in the
# core (core.tcl), whatever clocks it.
set core {*|kr1807vm1_core:core|}
set other [get_registers [list ${core}early* ${core}late* ${core}mr* ${core}t_count* \
                              ${core}bclr_pup ${core}bclr_reset]]
set acting [remove_from_collection [get_registers ${core}*] $other]
set latched [get_registers {*|kr1807vm1_bus:bus|data[*] *|kr1807vm1_bus:bus|ai[*]
                            *|kr1807vm1_bus:bus|ai_requests}]
set held [add_to_collection $acting $latched]
set_multicycle_path -setup 2 -from $held -to $acting
set_multicycle_path -hold 1 -from $held -to $acting
# The bus reads the core's request (req_*) only as it takes it, and the
# core's state has stood two ticks and more by then (five on the bench and
# on the board's 8 MHz: test_kr1807vm1_lockstep, test_machine): two ticks
# from the core's state to the bus's registers as well.
set bus [get_registers {*|kr1807vm1_bus:bus|*}]
set_multicycle_path -setup 2 -from $acting -to $bus
set_multicycle_path -hold 1 -from $acting -to $bus
