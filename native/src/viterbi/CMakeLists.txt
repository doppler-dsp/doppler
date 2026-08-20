# OBJECT library — pure C core, no Python dependency.
add_library(viterbi_core OBJECT viterbi_core.c)
target_include_directories(
  viterbi_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/viterbi)
target_link_libraries(viterbi_core PUBLIC
    conv_core)

add_executable(test_viterbi_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_viterbi_core.c)
target_link_libraries(test_viterbi_core
                      PRIVATE viterbi_core conv_core
    m)
target_include_directories(test_viterbi_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_viterbi_core COMMAND test_viterbi_core)

add_executable(
  bench_viterbi_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_viterbi_core.c)
target_link_libraries(bench_viterbi_core
                      PRIVATE viterbi_core conv_core
    m)
target_include_directories(
  bench_viterbi_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
