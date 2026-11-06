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

# Corr's bench rows, against the 1-D flavor, live in bench_correlator_core.c
# itself, because jm bench runs only bench_<component>_core: as their own
# executable they were built and never run. They share that file rather than
# a second source, because jm_bench.h's static functions warn unused in any
# translation unit that does not write the JSON (just-makeit#2497).
# `make bench-ab` pairs them with main's bench_corr_core by alias through
# step 3 (docs/design/correlator.md section 7.1). pn_core and cvt_core: the
# short rows (step 3.1b) correlate an m-sequence from the shipped generator,
# mapped by the shipped NRZ mapper.
target_link_libraries(bench_correlator_core PRIVATE pn_core cvt_core)

# The detector flavors' tests (#2752 step 3.3). They came with
# CorrDetector and CorrDetector2D when detector/ and detector2d/ were
# deleted, as Corr's did in step 3.2, so they are registered here.
#
# test_detector_equiv: the 1-D detector flavor against the 2-D one at one
# row, hit for hit (docs/design/correlator.md section 7). It ran first on
# the retired detector/ and detector2d/.
foreach(_t test_detector_equiv test_correlator_detector_1d
           test_correlator_detector_2d)
  add_executable(${_t} ${CMAKE_SOURCE_DIR}/native/tests/correlator/${_t}.c)
  target_link_libraries(${_t} PRIVATE correlator_core Threads::Threads
                                      fft2d_core fft_core ${JM_MATH_LIBRARY})
  target_include_directories(${_t} PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                           ${CMAKE_SOURCE_DIR}/native/tests)
  add_test(NAME ${_t} COMMAND ${_t})
endforeach()
