# OBJECT library — pure C core, no Python dependency.
add_library(dp_interrupt_guard_core OBJECT dp_interrupt_guard_core.c)
target_include_directories(
  dp_interrupt_guard_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/dp_interrupt_guard)
target_link_libraries(dp_interrupt_guard_core PUBLIC
    $<TARGET_OBJECTS:dp_interrupt_obj>)

add_executable(test_dp_interrupt_guard_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_dp_interrupt_guard_core.c)
target_link_libraries(test_dp_interrupt_guard_core
                      PRIVATE dp_interrupt_guard_core $<TARGET_OBJECTS:dp_interrupt_obj>
    m)
target_include_directories(test_dp_interrupt_guard_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_dp_interrupt_guard_core COMMAND test_dp_interrupt_guard_core)

add_executable(
  bench_dp_interrupt_guard_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_dp_interrupt_guard_core.c)
target_link_libraries(bench_dp_interrupt_guard_core
                      PRIVATE dp_interrupt_guard_core $<TARGET_OBJECTS:dp_interrupt_obj>
    m)
target_include_directories(
  bench_dp_interrupt_guard_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
