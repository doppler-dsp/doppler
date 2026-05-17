add_library(resamp_core OBJECT resamp_core.c)
target_include_directories(resamp_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/resamp)
target_link_libraries(resamp_core PUBLIC m)

add_executable(test_resamp_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_resamp_core.c)
target_link_libraries(test_resamp_core PRIVATE resamp_core m)
target_include_directories(test_resamp_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_resamp_core COMMAND test_resamp_core)

add_executable(bench_resamp_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_resamp_core.c)
target_link_libraries(bench_resamp_core PRIVATE resamp_core m)
target_include_directories(bench_resamp_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
