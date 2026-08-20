# OBJECT library — pure C core, no Python dependency.
add_library(conv_enc_core OBJECT conv_enc_core.c)
target_include_directories(
  conv_enc_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/conv_enc)
target_link_libraries(conv_enc_core PUBLIC
    conv_core)

add_executable(test_conv_enc_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_conv_enc_core.c)
target_link_libraries(test_conv_enc_core
                      PRIVATE conv_enc_core conv_core
    m)
target_include_directories(test_conv_enc_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_conv_enc_core COMMAND test_conv_enc_core)

add_executable(
  bench_conv_enc_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_conv_enc_core.c)
target_link_libraries(bench_conv_enc_core
                      PRIVATE conv_enc_core conv_core
    m)
target_include_directories(
  bench_conv_enc_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
