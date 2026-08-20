# conv — hand-owned c_dep: convolutional codes, both directions.
#
# Its own c_dep rather than a library inside `ccsds_tm`, because the code
# family is not CCSDS's: `ccsds_tm` holds a CONFIGURATION of it
# (CCSDS_TM_CONV) the way any other caller would. A decoder that lived under
# `ccsds_tm` would put the whole channel-coding chain on the link line of
# anyone who wanted a trellis.
#
# Pure-C OBJECT library, no CPython. jm apply only emits the add_subdirectory
# for this dir (listed in [project].c_deps); this file is hand-maintained.

add_library(conv_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/conv/conv_core.c)
target_include_directories(conv_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

add_executable(test_conv_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_conv_core.c)
# No ccsds_tm: the test's external truth is the impulse response, which is
# what a
# generator polynomial means and is checkable for every configuration. Holding
# the CCSDS numbers to the standard is test_ccsds_tm_conv.c's job.
target_link_libraries(test_conv_core PRIVATE conv_core m)
target_include_directories(test_conv_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_conv_core COMMAND test_conv_core)

# The benchmark. Hand-registered because `conv` is a c_dep and not one of
# jm's components, so `jm apply` generates no target for it and `jm bench`
# cannot run it -- just-makeit#1023; run it by hand meanwhile.
# scripts/check_bench_coverage.py keeps the rest honest: the target exists,
# it records a measurement, and it writes JSON under a collectable name.
add_executable(bench_conv_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_conv_core.c)
target_link_libraries(bench_conv_core PRIVATE conv_core m)
target_include_directories(bench_conv_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/benchmarks)
