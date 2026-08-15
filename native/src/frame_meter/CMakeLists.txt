# OBJECT library — pure C core, no Python dependency.
add_library(frame_meter_core OBJECT frame_meter_core.c)
target_include_directories(
  frame_meter_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/frame_meter)
target_link_libraries(frame_meter_core PUBLIC
    ber_core
    ber_meter_core
    detection_core)

add_executable(test_frame_meter_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_frame_meter_core.c)
target_link_libraries(test_frame_meter_core
                      PRIVATE frame_meter_core ber_core
    ber_meter_core
    detection_core
    m)
target_include_directories(test_frame_meter_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_frame_meter_core COMMAND test_frame_meter_core)

add_executable(
  bench_frame_meter_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_frame_meter_core.c)
target_link_libraries(bench_frame_meter_core
                      PRIVATE frame_meter_core ber_core
    ber_meter_core
    detection_core
    m)
target_include_directories(
  bench_frame_meter_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
