add_library(resamp_core OBJECT resamp_core.c)
target_include_directories(resamp_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/doppler/resamp)
target_link_libraries(resamp_core PUBLIC ${DP_MATH_LIBRARY})

add_executable(test_resamp_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_resamp_core.c)
# resample_core only for the TEST: §21 holds the built-in bank's sizing and
# beta to the public dp_kaiser_* functions (doppler#2652). resamp_core itself
# uses the header-inline formulas and links nothing new.
target_link_libraries(test_resamp_core PRIVATE resamp_core resample_core
    ${DP_MATH_LIBRARY})
target_include_directories(test_resamp_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_resamp_core COMMAND test_resamp_core)

add_executable(bench_resamp_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_resamp_core.c)
target_link_libraries(bench_resamp_core PRIVATE resamp_core ${DP_MATH_LIBRARY})
target_include_directories(bench_resamp_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
