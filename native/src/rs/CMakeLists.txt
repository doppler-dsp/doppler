# rs — hand-owned c_dep: Reed-Solomon over GF(2^J), any field, any root set.
#
# Its own c_dep rather than a library inside `ccsds_tm`, for the reason `conv`
# is: the code family is not CCSDS's. `ccsds_tm` holds a CONFIGURATION of it
# (CCSDS_TM_RS) the way any other caller would, together with the dual basis
# and the interleaver, which are the standard's and not the code's.
#
# Pure-C OBJECT library, no CPython. jm apply only emits the add_subdirectory
# for this dir (listed in [project].c_deps); this file is hand-maintained.

add_library(rs_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/rs/rs_core.c)
target_include_directories(rs_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

add_executable(test_rs_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_rs_core.c)
# No ccsds_tm: this test's external truth is the code's own distance -- E
# errors corrected, E+1 never recovered -- which holds for every
# configuration.
# Holding the CCSDS numbers to Annex G is test_ccsds_tm_rs.c's job.
target_link_libraries(test_rs_core PRIVATE rs_core m)
target_include_directories(test_rs_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_rs_core COMMAND test_rs_core)

# The benchmark. Hand-registered because `rs` is a c_dep and not one of jm's
# components -- see the note in native/src/conv/CMakeLists.txt.
add_executable(bench_rs_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_rs_core.c)
target_link_libraries(bench_rs_core PRIVATE rs_core m)
target_include_directories(bench_rs_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/benchmarks)
