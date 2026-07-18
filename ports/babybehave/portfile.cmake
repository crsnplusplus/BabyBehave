# Overlay port (not yet in the curated vcpkg registry - see the "vcpkg"
# subsection of README.md's Installation section for how to use this).
#
# SHA512 is the placeholder value "0": there is no tagged v0.7.19 GitHub
# release yet for a non-`--head` install to pin against. Right now this
# port only works via `--head` (clones HEAD_REF directly, no archive/hash
# needed). Once a v0.7.19 tag exists, update REF below and run
# `vcpkg install babybehave --overlay-ports=<path to this directory>` once -
# it will fail with the real SHA512 to paste in here, same as authoring any
# new vcpkg port.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO crsnplusplus/BabyBehave
    REF "v${VERSION}"
    SHA512 0
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME BabyBehave
    CONFIG_PATH lib/cmake/BabyBehave
)

# Header-only: nothing under debug/ but an empty (or headers-only) tree.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
