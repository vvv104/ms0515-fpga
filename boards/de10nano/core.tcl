# Stage 7d (docs/plan.md): the MS 0515 as a MiSTer core for the DE10-Nano.
#
#   quartus_sh -t boards/de10nano/core.tcl [map]
#
# builds the project in build/mister: Template_MiSTer's sys/ (the submodule
# in boards/de10nano/template) copied in as it is - MiSTer's scripts find
# it beside the project - with the template's project settings, and the
# core's own files in place of the template's example core: the chips, the
# board as the machine takes it, the parts, the machine and its emu.  Runs
# analysis and synthesis (map) or the whole flow, the .rbf in
# build/mister/output_files.
package require ::quartus::project
package require ::quartus::flow

set here [file dirname [file normalize [info script]]]
set root [file normalize [file join $here .. ..]]
set template [file join $here template]
set work [file join $root build mister]
file mkdir $work
cd $work
file delete -force sys
file copy [file join $template sys] sys

project_new ms0515 -overwrite
# the template's settings, line by line, but its example core's files
set f [open [file join $template Template.qsf]]
foreach line [split [read $f] "\n"] {
    if {[string match "source files.qip*" $line]} {
        continue
    }
    eval $line
}
close $f

# the К573РФ4's 8192 cells are filled by a loop (rtl/chips/k573rf4.sv)
set_global_assignment -name VERILOG_CONSTANT_LOOP_LIMIT 10000
set sources {}
foreach dir {chips parts} {
    set all [lsort [glob [file join $root rtl $dir *.sv]]]
    set packages [lsearch -all -inline $all *_pkg.sv]
    lappend sources {*}$packages {*}[lsearch -all -inline -not $all *_pkg.sv]
}
lappend sources [file join $root rtl board ns4_machine.sv] [file join $root rtl board ms7004_machine.sv] \
    [file join $root rtl ms0515.sv] [file join $here ms0515_emu.sv]
foreach s $sources {
    set_global_assignment -name SYSTEMVERILOG_FILE $s
}
# sys/ takes the core's PLL from rtl/pll.qip
file mkdir rtl
set f [open [file join rtl pll.qip] w]
puts $f "set_global_assignment -name SYSTEMVERILOG_FILE [file join $here pll.sv]"
close $f
set_global_assignment -name SDC_FILE [file join $template Template.sdc]
set_global_assignment -name SDC_FILE [file join $here kr1807vm1.sdc]
project_close

# build_id.v (the menu's date) and jtag.cdf, as the template's pre-flow
# script makes them - it runs before a whole flow, not before map alone
exec [file join $::quartus(binpath) quartus_sh] -t sys/build_id.tcl compile ms0515 ms0515
project_open ms0515

if {[lindex $argv 0] eq "map"} {
    execute_module -tool map
} else {
    execute_flow -compile
}
project_close
