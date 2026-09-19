# Chainloaded by triplets/x64-windows-clangcl.cmake: name the compiler, then
# defer to vcpkg's own Windows toolchain for every flag (CRT linkage, /Z7,
# charset), so this build differs from a stock x64-windows one in the
# compiler and nothing else.
#
# vcpkg builds ports in a SCRUBBED environment: PATH is rebuilt from the
# system directories plus vcvars, so a clang-cl that resolves in your shell
# is not on it. Hence a search rather than a bare name. In order: an explicit
# LLVMInstallDir (the variable Visual Studio itself honours; the triplet
# passes it through), the standalone LLVM installer's location, then the
# "C++ Clang tools for Windows" component inside Visual Studio.
find_program(DOPPLER_CLANG_CL
    NAMES clang-cl
    HINTS
        "$ENV{LLVMInstallDir}/bin"
        "$ENV{ProgramFiles}/LLVM/bin"
        "$ENV{VCINSTALLDIR}Tools/Llvm/x64/bin"
    REQUIRED)
set(CMAKE_C_COMPILER "${DOPPLER_CLANG_CL}")
set(CMAKE_CXX_COMPILER "${DOPPLER_CLANG_CL}")

# vcpkg passes its root as -D_VCPKG_ROOT_DIR, which a try_compile project
# re-reading this file would not see unless it is forwarded.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES _VCPKG_ROOT_DIR)
include("${_VCPKG_ROOT_DIR}/scripts/toolchains/windows.cmake")
