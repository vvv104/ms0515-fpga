# Test benches: one executable per chip module, verilated from rtl/chips,
# plus the reference model's own check.  doctest supplies main().

add_library(sim_main STATIC main.cpp)
target_link_libraries(sim_main PUBLIC doctest::doctest)
ms0515_strict(sim_main)

# ms0515_chip_test(<chip>): the module <chip> of rtl/chips against
# chips/test_<chip>.cpp.  All chip sources are given, so a type may use
# another's module.  The verilated model is its own library (V<chip>).
file(GLOB MS0515_CHIP_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/rtl/chips/*.sv")
function(ms0515_chip_test chip)
    set(model V${chip})
    add_library(${model} STATIC)
    set_target_properties(${model} PROPERTIES SYSTEM ON)
    # time comes from the VerilatedContext, not a global sc_time_stamp()
    target_compile_definitions(${model} PUBLIC VL_TIME_CONTEXT)
    verilate(${model}
        SOURCES ${MS0515_CHIP_SOURCES}
        TOP_MODULE ${chip}
        PREFIX ${model}
        VERILATOR_ARGS --language ${MS0515_SV_LANGUAGE} -Wall)
    set(target test_${chip})
    add_executable(${target} chips/test_${chip}.cpp)
    target_link_libraries(${target} PRIVATE sim_main ${model})
    ms0515_strict(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

foreach(chip
        k555la3 k555la4 k555la2 k555la13 k555li1 k555li3 k555li6 k555ll1
        kr531le1 k555lp5 k555ln1 k555ln2 k155ln5 k155lp9 k555tl2 k555lr11
        k555id7 kr531id14 k555kp2 k555kp11 k555kp15 k555iv3 k555ap3 k555ap5
        k555tm2 kr531tm2 kr1533tm2 k555tm8 k555tm9 k555ir22 k555ir23 k555ir16
        k555ie10 k555ie19
        kr559ip11 kr1102ap15 k554sa3)
    ms0515_chip_test(${chip})
endforeach()

add_executable(test_reference reference/test_reference.cpp)
target_link_libraries(test_reference PRIVATE sim_main ms0515_core)
ms0515_strict(test_reference)
add_test(NAME test_reference COMMAND test_reference)
