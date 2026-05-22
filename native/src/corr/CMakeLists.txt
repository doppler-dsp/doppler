# OBJECT library — uses fft_core symbols; pocketfft resolved transitively.
add_library(corr_core OBJECT corr_core.c)
target_include_directories(corr_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/corr)
target_link_libraries(corr_core PUBLIC fft_core m)

add_executable(test_corr_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_corr_core.c)
target_include_directories(test_corr_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(test_corr_core PRIVATE corr_core fft_core m)
add_test(NAME test_corr_core COMMAND test_corr_core)

add_executable(bench_corr_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_corr_core.c)
target_include_directories(bench_corr_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(bench_corr_core PRIVATE corr_core fft_core m)
