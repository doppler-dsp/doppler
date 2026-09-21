# C-level test — buffer/buffer.h is header-only (no _core.c to link). This is
# the only buffer gate on the macOS C job, which runs without BUILD_PYTHON, so
# it lives outside the guard below.
# dp_interrupt_obj: buffer.h is header-only, but its wait() now consults the
# interrupt flag, whose one definition lives in the core. These targets embed
# objects rather than linking libdoppler, so they need it named here.
add_executable(test_buffer_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_buffer_core.c
               $<TARGET_OBJECTS:dp_interrupt_obj>)
# Threads: the end-of-stream ordering can only be exercised with a consumer
# already spinning while the producer writes and closes.
find_package(Threads REQUIRED)
target_link_libraries(test_buffer_core PRIVATE Threads::Threads)
target_include_directories(test_buffer_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_buffer_core COMMAND test_buffer_core)

# dp_parallel.h is header-only too (the bounded parallel-for and the
# persistent pool), so its test lives beside buffer's for the same reason:
# nothing to link, and this CMakeLists is hand-owned. Threads for real.
add_executable(test_dp_parallel
               ${CMAKE_SOURCE_DIR}/native/tests/test_dp_parallel.c)
target_link_libraries(test_dp_parallel PRIVATE Threads::Threads)
target_include_directories(test_dp_parallel
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_dp_parallel COMMAND test_dp_parallel)

# Benchmark — same story as the test above: header-only, so there is no
# component core to link, and this CMakeLists is hand-owned (the module is
# no_generate), so the target is written here rather than by `jm apply`.
add_executable(bench_buffer_core
               ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_buffer_core.c
               $<TARGET_OBJECTS:dp_interrupt_obj>)
# libm because jm_bench.h's stats take a sqrt for the standard deviation.
# A Release build folds that call away and links without it; a Debug one
# (make test-tsan, make test-ubsan) does not, which is where this surfaced.
target_link_libraries(bench_buffer_core PRIVATE ${DP_MATH_LIBRARY})
target_include_directories(bench_buffer_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

# TEMPORARY (just-buildit/just-makeit#1432): the ring objects cannot declare
# `depends_on dp_interrupt_guard` while they are header_only, so the symbols
# buffer.h calls are embedded by hand. Delete with the TODO in
# just-makeit.toml's [module.buffer].
if(TARGET buffer)
  target_sources(buffer PRIVATE $<TARGET_OBJECTS:dp_interrupt_obj>)
endif()
