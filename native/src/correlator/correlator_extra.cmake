# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# The 1-D flavor's own test, bench and equivalence test (#2752 step 3.2).
# jm generates one test and one bench per component, test_correlator_core
# and bench_correlator_core, which are Corr2D's. The 1-D flavor's came with
# Corr when corr/ was deleted, so they are registered here.

# test_corr_equiv: the 1-D flavor against the 2-D one at one row, byte for
# byte (docs/design/correlator.md section 2.1).
add_executable(test_corr_equiv
               ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_corr_equiv.c)
target_link_libraries(test_corr_equiv
                      PRIVATE correlator_core Threads::Threads fft2d_core
                              fft_core ${JM_MATH_LIBRARY})
target_include_directories(test_corr_equiv
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_corr_equiv COMMAND test_corr_equiv)

# test_correlator_1d: Corr's C tests, run against the 1-D flavor.
add_executable(
  test_correlator_1d
  ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_correlator_1d.c
  ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_correlator_1d_symbols.c)
target_link_libraries(test_correlator_1d
                      PRIVATE correlator_core Threads::Threads fft2d_core
                              fft_core ${JM_MATH_LIBRARY})
target_include_directories(test_correlator_1d
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_correlator_1d COMMAND test_correlator_1d)

# bench_correlator_1d.c: Corr's bench, against the 1-D flavor. It is a
# second source of jm's bench_correlator_core, because jm bench runs only
# bench_<component>_core: as its own executable it was built and never run.
# `make bench-ab` pairs its rows with main's bench_corr_core by alias
# through step 3 (docs/design/correlator.md section 7.1). pn_core and
# cvt_core: the short rows (step 3.1b) correlate an m-sequence from the
# shipped generator, mapped by the shipped NRZ mapper.
target_sources(
  bench_correlator_core
  PRIVATE ${CMAKE_SOURCE_DIR}/native/benchmarks/correlator/bench_correlator_1d.c)
target_link_libraries(bench_correlator_core PRIVATE pn_core cvt_core)
