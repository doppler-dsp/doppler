# x64 Windows, compiled by clang-cl against the MSVC runtime -- the one
# Windows toolchain that builds doppler (cl.exe has no C99 complex).
#
# NOT `set(VCPKG_PLATFORM_TOOLSET ClangCL)`: vcpkg matches that variable
# against the Visual Studio toolset VERSIONS it discovered (v142, v143, ...)
# and stops with "no Visual Studio instance" for anything else; and it only
# ever reaches CMake as `-T`, which the Ninja generator vcpkg uses ignores.
# `-T ClangCL` is the MSBuild spelling. Under Ninja the compiler is chosen
# the way Visual Studio's own CMake integration chooses it: by naming it.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# A chainloaded toolchain turns the vcvars environment OFF by default, and
# clang-cl needs it: INCLUDE, LIB and the linker come from there.
set(VCPKG_LOAD_VCVARS_ENV ON)
# Where to find clang-cl when it is not in a default location; see the
# toolchain file for the search order.
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED LLVMInstallDir)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../toolchains/clang-cl.cmake")
