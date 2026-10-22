"""Conan recipe for the MS 0515 FPGA project's test benches.

    conan create recipes/verilator --version 5.044   (once; ConanCenter has no Verilator)
    conan build . --build=missing
    conan build . --build=missing -o "&:corner=max"   (or min: docs/timing.md)

Verilator, CMake and Ninja come from Conan, so a build machine needs a C++
compiler and Conan only.  The emulator (the reference model) is found
through MS0515_EMULATOR_DIR, falling back to ../ms0515 beside this
repository.
"""
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class Ms0515FpgaRecipe(ConanFile):
    name = "ms0515-fpga"
    settings = "os", "arch", "compiler", "build_type"
    # the chips' delays the board is generated with: the table's typical
    # ones (rtl/board/ns4.sv), or a corner of them, built apart in a folder
    # of its own (tools/board.py corner)
    options = {"corner": ["typ", "max", "min"]}
    default_options = {"corner": "typ"}

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25 <5]")
        self.tool_requires("ninja/[>=1.11]")
        self.tool_requires("verilator/5.044")

    def requirements(self):
        self.requires("doctest/2.4.11")

    def layout(self):
        cmake_layout(self)
        if self.options.corner != "typ":
            self.folders.build = f"build/{self.options.corner}"
            self.folders.generators = f"build/{self.options.corner}/generators"

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self, generator="Ninja")
        verilator = self.dependencies.build["verilator"].package_folder
        tc.cache_variables["verilator_DIR"] = verilator.replace("\\", "/")
        if self.options.corner != "typ":
            board = os.path.join(self.build_folder, "board")
            tool = os.path.join(self.source_folder, "tools", "board.py")
            self.run(f'python "{tool}" corner {self.options.corner} "{board}"')
            tc.cache_variables["MS0515_BOARD_SV"] = os.path.join(board, "ns4.sv").replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.ctest(cli_args=["--output-on-failure"])
