# pocketfft is C++ (templates, std::complex).  CXX is declared in the
# top-level project; CXX_STANDARD is pinned on pocketfft_cxx so it
# does not bleed into the rest of the project.
add_library(pocketfft_cxx STATIC pocketfft.cc)
set_target_properties(pocketfft_cxx PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
target_include_directories(pocketfft_cxx PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(pocketfft_cxx PUBLIC stdc++)

# OBJECT library — pure C (C11); links the pocketfft_cxx static lib.
add_library(fft_core OBJECT fft_core.c)
target_include_directories(fft_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/fft)
target_link_libraries(fft_core PUBLIC m pocketfft_cxx)

add_executable(test_fft_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_fft_core.c)
target_include_directories(test_fft_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(test_fft_core PRIVATE fft_core m)
add_test(NAME test_fft_core COMMAND test_fft_core)

add_executable(bench_fft_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_fft_core.c)
target_include_directories(bench_fft_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(bench_fft_core PRIVATE fft_core m)
