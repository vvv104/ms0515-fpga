"""Conan recipe for Verilator, which ConanCenter does not carry.

    conan create recipes/verilator --version 5.044

Builds Verilator from its upstream release with its own CMake build and
packages the install tree as it is: bin/verilator_bin, include/ (the
runtime the generated models compile against) and verilator-config.cmake,
whose verilate() calls verilator_bin directly - no Perl at use time.
Consumers take it as a tool_requires and find_package(verilator CONFIG).
"""
import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import get

SOURCES = {
    "5.044": {
        "url": "https://github.com/verilator/verilator/archive/refs/tags/v5.044.tar.gz",
        "sha256": "ded2a4a96e3b836ddc9fd5d01127999d981adee4d19133ff819b7129897d801a",
    },
}


class VerilatorConan(ConanFile):
    name = "verilator"
    description = "Verilator, the SystemVerilog simulator that compiles to C++"
    license = "LGPL-3.0-only OR Artistic-2.0"
    url = "https://verilator.org"
    settings = "os", "arch", "compiler", "build_type"
    package_type = "application"

    def layout(self):
        cmake_layout(self, src_folder="src")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.25 <5]")
        self.tool_requires("ninja/[>=1.11]")
        if self.settings.os == "Windows":
            self.tool_requires("winflexbison/2.5.25")
        else:
            self.tool_requires("flex/2.6.4")
            self.tool_requires("bison/3.8.2")

    def source(self):
        get(self, **SOURCES[str(self.version)], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self, generator="Ninja")
        tc.cache_variables["OBJCACHE_ENABLED"] = False
        if self.settings.os == "Windows":
            flex = self.dependencies.build["winflexbison"]
            tc.cache_variables["WIN_FLEX_BISON"] = flex.package_folder.replace("\\", "/")
        tc.generate()

    def build(self):
        if self.settings.os == "Windows":
            os.environ["WIN_FLEX_BISON"] = self.dependencies.build["winflexbison"].package_folder
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = ["."]
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.buildenv_info.define_path("VERILATOR_ROOT", self.package_folder)
        self.runenv_info.define_path("VERILATOR_ROOT", self.package_folder)
