# OBJECT library — pure C core, no Python dependency.
add_library(lo_core OBJECT lo_core.c)
target_include_directories(lo_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/lo)

add_executable(test_lo_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_lo_core.c)
target_link_libraries(test_lo_core PRIVATE lo_core m)
target_include_directories(test_lo_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_lo_core COMMAND test_lo_core)

add_executable(bench_lo_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_lo_core.c)
target_link_libraries(bench_lo_core PRIVATE lo_core m)
target_include_directories(bench_lo_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
