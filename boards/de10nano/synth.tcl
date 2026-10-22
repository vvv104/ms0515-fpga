# Stage 7a (docs/plan.md): the НС4 board alone through Quartus for the
# DE10-Nano's Cyclone V, to see that it synthesises, what it takes, and
# whether it meets the 120 MHz master clock.
#
#   quartus_sh -t boards/de10nano/synth.tcl [map]
#
# builds the project in build/quartus and runs analysis and synthesis
# (map) or the whole flow up to timing analysis (the default).  The
# board's ports are not the FPGA's pins - a MiSTer core wraps it - so they
# are made virtual pins: the fit places the logic alone.
package require ::quartus::project
package require ::quartus::flow

set here [file dirname [file normalize [info script]]]
set root [file normalize [file join $here .. ..]]
set work [file join $root build quartus]
file mkdir $work
cd $work

project_new ns4 -overwrite
set_global_assignment -name FAMILY "Cyclone V"
set_global_assignment -name DEVICE 5CSEBA6U23I7
set_global_assignment -name TOP_LEVEL_ENTITY ns4
set_global_assignment -name NUM_PARALLEL_PROCESSORS ALL
# the К573РФ4's 8192 cells are filled by a loop (rtl/chips/k573rf4.sv)
set_global_assignment -name VERILOG_CONSTANT_LOOP_LIMIT 10000
set_global_assignment -name SEARCH_PATH [file join $root rtl board]
foreach f [lsort [glob [file join $root rtl chips *.sv]]] {
    set_global_assignment -name SYSTEMVERILOG_FILE $f
}
set_global_assignment -name SYSTEMVERILOG_FILE [file join $root rtl board ns4.sv]
set_global_assignment -name SDC_FILE [file join $here ns4.sdc]
# Registers moved through the logic between them, cycle for cycle the same
# behaviour: the delay lines' stages are where the slack is.
set_global_assignment -name PHYSICAL_SYNTHESIS_REGISTER_RETIMING ON
set_global_assignment -name PHYSICAL_SYNTHESIS_REGISTER_DUPLICATION ON
set_global_assignment -name PHYSICAL_SYNTHESIS_COMBO_LOGIC ON
set_global_assignment -name OPTIMIZATION_MODE "HIGH PERFORMANCE EFFORT"
foreach ports {x_* nx_* n_* contention*} {
    set_instance_assignment -name VIRTUAL_PIN ON -to $ports
}

if {[lindex $argv 0] eq "map"} {
    execute_module -tool map
} else {
    execute_flow -compile
}
project_close
