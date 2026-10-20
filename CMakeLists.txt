cmake_minimum_required(VERSION 3.25)
project(ms0515_fpga LANGUAGES C CXX)

# The language standards are declared here once; every target inherits
# them.  SystemVerilog: the synthesizable subset of IEEE 1800-2017.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(MS0515_SV_LANGUAGE 1800-2017)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ms0515_strict(<target>): our own code builds with no warnings at all.
# The models Verilator generates and its runtime are a tool's output, built
# in their own libraries without these flags; their headers reach our code
# as system headers (the SYSTEM target property), so nothing is silenced.
function(ms0515_strict target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

find_package(verilator REQUIRED CONFIG)
find_package(doctest REQUIRED CONFIG)

# The emulator, whose core is the reference model.
if(DEFINED ENV{MS0515_EMULATOR_DIR})
    set(MS0515_EMULATOR_DIR "$ENV{MS0515_EMULATOR_DIR}")
else()
    set(MS0515_EMULATOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ms0515")
endif()
if(NOT EXISTS "${MS0515_EMULATOR_DIR}/src/core/CMakeLists.txt")
    message(FATAL_ERROR "The emulator is not at ${MS0515_EMULATOR_DIR}: set MS0515_EMULATOR_DIR")
endif()
add_subdirectory("${MS0515_EMULATOR_DIR}/src/core" emulator_core)
ms0515_strict(ms0515_core)

enable_testing()
add_subdirectory(sim)
