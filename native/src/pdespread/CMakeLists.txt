# OBJECT library — pure C core, no Python dependency.
add_library(pdespread_core OBJECT pdespread_core.c)
target_include_directories(
  pdespread_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/pdespread)
target_link_libraries(pdespread_core PUBLIC
    dll_core
    loop_filter_core)

add_executable(test_pdespread_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_pdespread_core.c)
target_link_libraries(test_pdespread_core
                      PRIVATE pdespread_core dll_core
    loop_filter_core
    m)
target_include_directories(test_pdespread_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_pdespread_core COMMAND test_pdespread_core)

add_executable(
  bench_pdespread_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_pdespread_core.c)
target_link_libraries(bench_pdespread_core
                      PRIVATE pdespread_core dll_core
    loop_filter_core
    m)
target_include_directories(
  bench_pdespread_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
