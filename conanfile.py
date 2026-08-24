from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class MissionHoldSkipperRecipe(ConanFile):
    name = "mission-hold-skipper"
    version = "0.1.0"
    description = "Hold-to-skip mission scenes with an RDR2 style ring, for GTA SA v1.0 US"
    license = "MIT"

    settings = "os", "compiler", "build_type", "arch"

    requires = [
        "mini/0.9.20",
    ]

    exports_sources = "CMakeLists.txt", "src/*", "MissionHoldSkipper.ini"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()

        tc = CMakeToolchain(self)
        tc.user_presets_path = "ConanPresets.json"
        tc.generate()