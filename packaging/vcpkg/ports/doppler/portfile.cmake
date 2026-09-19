# An OVERLAY port: it builds the checkout it lives in, not a downloaded
# archive. So there is no REF and no SHA512 to go stale, the port is tested
# on every PR against the source it will ship with, and a consumer gets the
# version they cloned:
#
#   git clone --branch v<x.y.z> https://github.com/doppler-dsp/doppler
#   vcpkg install doppler \
#       --overlay-ports=doppler/packaging/vcpkg/ports \
#       --overlay-triplets=doppler/packaging/vcpkg/triplets \
#       --triplet x64-windows-clangcl
#
# Not upstream microsoft/vcpkg: its default x64-windows triplet compiles with
# cl.exe, which cannot build doppler and cannot even include its headers
# (native/inc/dp_complex.h #errors on it). The clang-cl triplet beside this
# port is the supported Windows toolchain.
get_filename_component(SOURCE_PATH
    "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

# doppler's export set carries BOTH linkages (doppler::doppler and
# doppler::doppler-static), and the generated doppler-targets.cmake refuses
# to load if any imported file is missing. A static triplet must ship no DLL,
# which would delete one -- so until the build can emit a single linkage,
# only the dynamic layout is expressible (doppler#1405).
vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)

# The same three switches `make package-c` passes: that recipe is what the
# release archive is built from and what `make package-c-smoke` proves.
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_PYTHON=OFF
        -DCMAKE_INSTALL_LIBDIR=lib
)
vcpkg_cmake_install()

# lib/cmake/doppler -> share/doppler, debug and release targets merged.
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/doppler)
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

# The wfmgen CLI is POSIX-only today (it links the sigaction interrupt
# layer); where it exists, vcpkg keeps executables under tools/, not bin/.
if(NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_copy_tools(TOOL_NAMES wfmgen AUTO_CLEAN)
endif()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
