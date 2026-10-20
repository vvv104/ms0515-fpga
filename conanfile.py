"""Conan recipe for the MS 0515 FPGA project's test benches.

    conan create recipes/verilator --version 5.044   (once; ConanCenter has no Verilator)
    conan build . --build=missing

Verilator, CMake and Ninja come from Conan, so a build machine needs a C++
compiler and Conan only.  The emulator (the reference model) is found
through MS0515_EMULATOR_DIR, falling back to ../ms0515 beside this
repository.
"""
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class Ms0515FpgaRecipe(ConanFile):
    name = "ms0515-fpga"
    settings = "os", "arch", "compiler", "build_type"

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25 <5]")
        self.tool_requires("ninja/[>=1.11]")
        self.tool_requires("verilator/5.044")

    def requirements(self):
        self.requires("doctest/2.4.11")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self, generator="Ninja")
        verilator = self.dependencies.build["verilator"].package_folder
        tc.cache_variables["verilator_DIR"] = verilator.replace("\\", "/")
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.ctest(cli_args=["--output-on-failure"])
