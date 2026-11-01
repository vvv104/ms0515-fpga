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
        kr559ip11 kr1102ap15 k554sa3 kr565ru5 kr580vi53 kr580vv55 kr580vv51
        k155ln3 k155tm2 k1102lp1 k564kp2 k589ir12 kr580vr43)
    ms0515_chip_test(${chip})
endforeach()

# a crystal: the 8 MHz one by default, the 15 MHz one by its parameters
ms0515_model(Vcrystal "${MS0515_CHIPS}/crystal.sv" crystal)
ms0515_model(Vcrystal_15 "${MS0515_CHIPS}/crystal.sv" crystal ARGS -GPERIOD=8 -GHIGH=4)
ms0515_test(test_crystal chips/test_crystal.cpp Vcrystal Vcrystal_15)
ms0515_model(Vcrystal_frac "${MS0515_CHIPS}/crystal_frac.sv" crystal_frac)
ms0515_test(test_crystal_frac chips/test_crystal_frac.cpp Vcrystal_frac)

# a bank of delay lines: three ticks, one bit or three
ms0515_model(Vdelay_bank "${MS0515_CHIPS}/delay_bank.sv" delay_bank ARGS -GTICKS=3)
ms0515_model(Vdelay_bank_asym "${MS0515_CHIPS}/delay_bank.sv" delay_bank ARGS -GTICKS=3 -GFALL=1)
ms0515_model(Vdelay_bank_wide "${MS0515_CHIPS}/delay_bank.sv" delay_bank ARGS -GTICKS=3 -GW=3)
ms0515_model(Vdelay_bank_long "${MS0515_CHIPS}/delay_bank.sv" delay_bank ARGS -GTICKS=18 -GFALL=16)
ms0515_model(Vdelay_bank_one "${MS0515_CHIPS}/delay_bank.sv" delay_bank ARGS -GTICKS=1)
ms0515_test(test_delay_bank chips/test_delay_bank.cpp Vdelay_bank Vdelay_bank_asym Vdelay_bank_wide
            Vdelay_bank_long Vdelay_bank_one)

# a PROM is tested programmed with a pattern (chips/kr556rt12_bench.sv)
ms0515_model(Vkr556rt12_bench "${CMAKE_CURRENT_SOURCE_DIR}/chips/kr556rt12_bench.sv" kr556rt12_bench)
ms0515_test(test_kr556rt12 chips/test_kr556rt12.cpp Vkr556rt12_bench)
ms0515_model(Vk573rf4_bench "${CMAKE_CURRENT_SOURCE_DIR}/chips/k573rf4_bench.sv" k573rf4_bench)
ms0515_test(test_k573rf4 chips/test_k573rf4.cpp Vk573rf4_bench)
ms0515_chip_test(km1816ve48 PACKAGES "${MS0515_CHIPS}/km1816ve48_pkg.sv" ARGS --public-flat-rw)
ms0515_model(Vk573rf2_bench "${CMAKE_CURRENT_SOURCE_DIR}/chips/k573rf2_bench.sv" k573rf2_bench)
ms0515_test(test_k573rf2 chips/test_k573rf2.cpp Vk573rf2_bench)

ms0515_chip_test(kr1818vg93 PACKAGES "${MS0515_CHIPS}/kr1818vg93_pkg.sv")
ms0515_chip_test(kr1807vm1_bus PACKAGES "${MS0515_CHIPS}/kr1807vm1_pkg.sv")
ms0515_chip_test(kr1807vm1 PACKAGES ${MS0515_CPU_PACKAGES})
# the processor with its registers readable, against the emulator's
ms0515_model(Vkr1807vm1_public "${MS0515_CHIPS}/kr1807vm1.sv" kr1807vm1
             PACKAGES ${MS0515_CPU_PACKAGES} ARGS --public-flat-rw)
ms0515_test(test_kr1807vm1_lockstep chips/test_kr1807vm1_lockstep.cpp Vkr1807vm1_public ms0515_core)

# The board generated from the netlist (tools/board.py writes rtl/board/ns4.sv),
# or a corner of its delays (conanfile.py's corner option).
set(MS0515_BOARD_SV "${PROJECT_SOURCE_DIR}/rtl/board/ns4.sv" CACHE FILEPATH "the board the benches run")
ms0515_model(Vns4 "${MS0515_BOARD_SV}" ns4
             PACKAGES ${MS0515_CPU_PACKAGES} "${MS0515_CHIPS}/kr1818vg93_pkg.sv"
             ARGS --savable)
ms0515_test(test_board board/test_board.cpp Vns4 ms0515_core)
target_compile_definitions(test_board PRIVATE MS0515_FIRMWARE_DIR="${PROJECT_SOURCE_DIR}/firmware")
if(NOT MS0515_BOARD_SV STREQUAL "${PROJECT_SOURCE_DIR}/rtl/board/ns4.sv")
    target_compile_definitions(test_board PRIVATE MS0515_DELAY_CORNER)
endif()

ms0515_test(test_reference reference/test_reference.cpp ms0515_core)

# the bench's parts as logic, for the MiSTer core (rtl/parts)
ms0515_model(Vns4_video "${PROJECT_SOURCE_DIR}/rtl/parts/ns4_video.sv" ns4_video)
ms0515_test(test_ns4_video parts/test_ns4_video.cpp Vns4_video)
ms0515_model(Vvideo_half "${PROJECT_SOURCE_DIR}/rtl/parts/video_half.sv" video_half)
ms0515_test(test_video_half parts/test_video_half.cpp Vvideo_half)
ms0515_model(Vmc5305 "${PROJECT_SOURCE_DIR}/rtl/parts/mc5305.sv" mc5305
             PACKAGES "${PROJECT_SOURCE_DIR}/rtl/parts/mc5305_pkg.sv" ARGS -y "${PROJECT_SOURCE_DIR}/rtl/parts")
ms0515_test(test_mc5305 parts/test_mc5305.cpp Vmc5305)
# ... and on the board's own levels
ms0515_test(test_board_video board/test_board_video.cpp Vns4 Vns4_video)

# the machine of the MiSTer core: the board inside it (rtl/board/ns4_machine.sv)
# with the parts, against the bench's board
ms0515_model(Vms0515 "${PROJECT_SOURCE_DIR}/rtl/ms0515.sv" ms0515
             PACKAGES ${MS0515_CPU_PACKAGES} "${MS0515_CHIPS}/kr1818vg93_pkg.sv"
                      "${PROJECT_SOURCE_DIR}/rtl/parts/mc5305_pkg.sv"
             ARGS -y "${PROJECT_SOURCE_DIR}/rtl/board" -y "${PROJECT_SOURCE_DIR}/rtl/parts" --public-flat-rw)
ms0515_test(test_machine board/test_machine.cpp Vns4 Vns4_video Vms0515)

# the МС 7004 keyboard generated from its netlist (rtl/board/ms7004.sv)
ms0515_model(Vms7004 "${PROJECT_SOURCE_DIR}/rtl/board/ms7004.sv" ms7004
             PACKAGES "${MS0515_CHIPS}/km1816ve48_pkg.sv" ARGS --public-flat-rw)
ms0515_test(test_ms7004 board/test_ms7004.cpp Vms7004)
add_executable(probe_ms7004 board/probe_ms7004.cpp)
target_link_libraries(probe_ms7004 PRIVATE Vms7004)
ms0515_strict(probe_ms7004)

# the floppy drive the bench plays: its tracks (sim/board/drive.h)
ms0515_test(test_drive board/test_drive.cpp)
ms0515_test(test_drive_mechanics board/test_drive_mechanics.cpp)
# the keyboard stub on XS2 (sim/board/keyboard_stub.h)
ms0515_test(test_keyboard_stub board/test_keyboard_stub.cpp)
# their snapshots (sim/board/snapshot.h)
ms0515_test(test_snapshot board/test_snapshot.cpp)

# the board model's speed (not a test): sim/board/bench_board.cpp
add_executable(bench_board board/bench_board.cpp)
target_link_libraries(bench_board PRIVATE Vns4)

# run_board: the board for a stretch of machine time - fetches against the
# emulator, frames off the video DAC's inputs (not a test)
add_executable(run_board board/run_board.cpp)
target_link_libraries(run_board PRIVATE Vns4 Vmc5305 ms0515_core)
target_compile_definitions(run_board PRIVATE MS0515_FIRMWARE_DIR="${PROJECT_SOURCE_DIR}/firmware")
