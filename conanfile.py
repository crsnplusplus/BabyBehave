from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy


class BabyBehaveConan(ConanFile):
    name = "babybehave"
    version = "0.9.0"
    license = "MIT"
    url = "https://github.com/crsnplusplus/BabyBehave"
    homepage = url
    description = "A minimalistic, header-only BDD framework for modern C++."
    topics = ("bdd", "testing", "header-only", "cpp23")

    package_type = "header-library"
    # No settings needed at package_id time: a header-only library produces
    # the same package regardless of build_type/compiler/arch. cppstd is
    # still validated (see validate() below), just not part of the ID.
    no_copy_source = True

    exports_sources = "include/*", "LICENSE"

    def package_id(self):
        self.info.clear()

    def validate(self):
        # bdd.hpp guards every C++23-only stdlib feature behind __cpp_lib_*
        # feature-test macros and falls back to C++17-compatible code paths
        # when they're unavailable (see README's "C++ standard support"
        # section) - so C++17 is the real floor, not C++23.
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, 17)

    def package(self):
        copy(self, "*.hpp",
             src=f"{self.source_folder}/include",
             dst=f"{self.package_folder}/include")
        copy(self, "LICENSE",
             src=self.source_folder,
             dst=f"{self.package_folder}/licenses")

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "BabyBehave")
        self.cpp_info.set_property("cmake_target_name", "BabyBehave::BabyBehave")
