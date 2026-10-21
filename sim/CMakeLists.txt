# Test benches: one executable per chip module, verilated from rtl/chips,
# the board, plus the reference model's own check.  doctest supplies main().

add_library(sim_main STATIC main.cpp)
target_link_libraries(sim_main PUBLIC doctest::doctest)
ms0515_strict(sim_main)

set(MS0515_CHIPS "${PROJECT_SOURCE_DIR}/rtl/chips")
# the packages of КР1807ВМ1, in the order they import each other
set(MS0515_CPU_PACKAGES "${MS0515_CHIPS}/kr1807vm1_pkg.sv" "${MS0515_CHIPS}/kr1807vm1_steps_pkg.sv")

# ms0515_model(<library> <top source> <top module> [PACKAGES ...] [ARGS ...]):
# a verilated model as its own library.  Verilator finds the modules the top
# uses in rtl/chips by their names (-y); the packages it imports are given,
# first.  The model and the Verilator runtime build without our warning
# flags and reach our code as system headers.
function(ms0515_model library source top)
    cmake_parse_arguments(M "" "" "PACKAGES;ARGS" ${ARGN})
    add_library(${library} STATIC)
    set_target_properties(${library} PROPERTIES SYSTEM ON)
    # time comes from the VerilatedContext, not a global sc_time_stamp()
    target_compile_definitions(${library} PUBLIC VL_TIME_CONTEXT)
    verilate(${library}
        SOURCES ${M_PACKAGES} "${source}"
        TOP_MODULE ${top}
        PREFIX ${library}
        VERILATOR_ARGS --language ${MS0515_SV_LANGUAGE} -Wall -y "${MS0515_CHIPS}" ${M_ARGS})
endfunction()

# ms0515_test(<name> <source> <libraries...>)
function(ms0515_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE sim_main ${ARGN})
    ms0515_strict(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ms0515_chip_test(<chip> [PACKAGES ...]): the module <chip> of rtl/chips
# against chips/test_<chip>.cpp.
function(ms0515_chip_test chip)
    ms0515_model(V${chip} "${MS0515_CHIPS}/${chip}.sv" ${chip} ${ARGN})
    ms0515_test(test_${chip} chips/test_${chip}.cpp V${chip})
endfunction()

foreach(chip
        k555la3 k555la4 k555la2 k555la13 k555li1 k555li3 k555li6 k555ll1
        kr531le1 k555lp5 k555ln1 k555ln2 k155ln5 k155lp9 k555tl2 k555lr11
        k555id7 kr531id14 k555kp2 k555kp11 k555kp15 k555iv3 k555ap3 k555ap5
        k555tm2 kr531tm2 kr1533tm2 k555tm8 k555tm9 k555ir22 k555ir23 k555ir16
        k555ie10 k555ie19
        kr559ip11 kr1102ap15 k554sa3 kr565ru5 kr580vi53)
    ms0515_chip_test(${chip})
endforeach()

# a PROM is tested programmed with a pattern (chips/kr556rt12_bench.sv)
ms0515_model(Vkr556rt12_bench "${CMAKE_CURRENT_SOURCE_DIR}/chips/kr556rt12_bench.sv" kr556rt12_bench)
ms0515_test(test_kr556rt12 chips/test_kr556rt12.cpp Vkr556rt12_bench)
ms0515_model(Vk573rf4_bench "${CMAKE_CURRENT_SOURCE_DIR}/chips/k573rf4_bench.sv" k573rf4_bench)
ms0515_test(test_k573rf4 chips/test_k573rf4.cpp Vk573rf4_bench)

ms0515_chip_test(kr1807vm1_bus PACKAGES "${MS0515_CHIPS}/kr1807vm1_pkg.sv")
ms0515_chip_test(kr1807vm1 PACKAGES ${MS0515_CPU_PACKAGES})
# the processor with its registers readable, against the emulator's
ms0515_model(Vkr1807vm1_public "${MS0515_CHIPS}/kr1807vm1.sv" kr1807vm1
             PACKAGES ${MS0515_CPU_PACKAGES} ARGS --public-flat-rw)
ms0515_test(test_kr1807vm1_lockstep chips/test_kr1807vm1_lockstep.cpp Vkr1807vm1_public ms0515_core)

# The board generated from the netlist (tools/board.py writes rtl/board/ns4.sv).
ms0515_model(Vns4 "${PROJECT_SOURCE_DIR}/rtl/board/ns4.sv" ns4 PACKAGES ${MS0515_CPU_PACKAGES})
ms0515_test(test_board board/test_board.cpp Vns4 ms0515_core)
target_compile_definitions(test_board PRIVATE MS0515_FIRMWARE_DIR="${PROJECT_SOURCE_DIR}/firmware")

ms0515_test(test_reference reference/test_reference.cpp ms0515_core)
