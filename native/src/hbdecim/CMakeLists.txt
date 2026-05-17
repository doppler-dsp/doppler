add_library(hbdecim_core OBJECT hbdecim_core.c)
target_include_directories(hbdecim_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/hbdecim)
target_link_libraries(hbdecim_core PUBLIC m)

add_library(hbdecim_r2c_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/resample/hbdecim_r2c_core.c)
target_include_directories(hbdecim_r2c_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/hbdecim)
target_link_libraries(hbdecim_r2c_core PUBLIC m)

add_executable(test_hbdecim_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_hbdecim_core.c)
target_link_libraries(test_hbdecim_core PRIVATE hbdecim_core m)
target_include_directories(test_hbdecim_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_hbdecim_core COMMAND test_hbdecim_core)

add_executable(bench_hbdecim_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_hbdecim_core.c)
target_link_libraries(bench_hbdecim_core PRIVATE hbdecim_core m)
target_include_directories(bench_hbdecim_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
