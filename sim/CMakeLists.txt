# Test benches: one executable per chip module, verilated from rtl/chips,
# plus the reference model's own check.  doctest supplies main().

add_library(sim_main STATIC main.cpp)
target_link_libraries(sim_main PUBLIC doctest::doctest)
ms0515_strict(sim_main)

# ms0515_chip_test(<chip>): rtl/chips/<chip>.sv against chips/test_<chip>.cpp.
# The verilated model is its own library (V<chip>); the test links it.
function(ms0515_chip_test chip)
    set(model V${chip})
    add_library(${model} STATIC)
    set_target_properties(${model} PROPERTIES SYSTEM ON)
    # time comes from the VerilatedContext, not a global sc_time_stamp()
    target_compile_definitions(${model} PUBLIC VL_TIME_CONTEXT)
    verilate(${model}
        SOURCES "${PROJECT_SOURCE_DIR}/rtl/chips/${chip}.sv"
        TOP_MODULE ${chip}
        PREFIX ${model}
        VERILATOR_ARGS --language ${MS0515_SV_LANGUAGE} -Wall)
    set(target test_${chip})
    add_executable(${target} chips/test_${chip}.cpp)
    target_link_libraries(${target} PRIVATE sim_main ${model})
    ms0515_strict(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

ms0515_chip_test(k555la3)

add_executable(test_reference reference/test_reference.cpp)
target_link_libraries(test_reference PRIVATE sim_main ms0515_core)
ms0515_strict(test_reference)
add_test(NAME test_reference COMMAND test_reference)
