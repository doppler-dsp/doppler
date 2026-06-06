# OBJECT library — HalfbandDecimator adapter over hbdecim_core (defined
# in native/src/hbdecim/CMakeLists.txt, included by the root CMakeLists).
add_library(HalfbandDecimator_core OBJECT HalfbandDecimator_core.c)
target_include_directories(HalfbandDecimator_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/HalfbandDecimator
    ${CMAKE_SOURCE_DIR}/native/inc/hbdecim)

target_link_libraries(HalfbandDecimator_core PUBLIC
    hbdecim_core)
add_executable(test_HalfbandDecimator_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_HalfbandDecimator_core.c)
target_link_libraries(test_HalfbandDecimator_core
    PRIVATE HalfbandDecimator_core hbdecim_core m)
target_include_directories(test_HalfbandDecimator_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_HalfbandDecimator_core COMMAND test_HalfbandDecimator_core)

add_executable(bench_HalfbandDecimator_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_HalfbandDecimator_core.c)
target_link_libraries(bench_HalfbandDecimator_core
    PRIVATE HalfbandDecimator_core hbdecim_core m)
target_include_directories(bench_HalfbandDecimator_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
