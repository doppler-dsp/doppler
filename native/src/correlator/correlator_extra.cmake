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
                              fft_core detection_core dp_tlm_core cvt_core
                              ${JM_MATH_LIBRARY})
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
                              fft_core detection_core dp_tlm_core cvt_core
                              ${JM_MATH_LIBRARY})
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
# test_correlator_create: the one constructor (#2752 step 4a) builds each
# flavor as its own constructor does, and refuses what it would not honor.
foreach(_t test_detector_equiv test_correlator_detector_1d
           test_correlator_detector_2d test_correlator_create)
  add_executable(${_t} ${CMAKE_SOURCE_DIR}/native/tests/correlator/${_t}.c)
  target_link_libraries(${_t} PRIVATE correlator_core Threads::Threads
                                      fft2d_core fft_core detection_core dp_tlm_core cvt_core
                              ${JM_MATH_LIBRARY})
  target_include_directories(${_t} PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                           ${CMAKE_SOURCE_DIR}/native/tests)
  add_test(NAME ${_t} COMMAND ${_t})
endforeach()

# test_search_equiv: the search flavors' scenes, each against its own plain
# run (#2752 step 3.4b). They were the transition commit's acq-against-
# correlator oracle; acq/ is gone, and they pin that how a stream arrives
# (chunks, threads, a restore) changes no byte, under the library's flags.
add_executable(test_search_equiv
               ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_search_equiv.c)
target_link_libraries(test_search_equiv
                      PRIVATE correlator_core Threads::Threads fft2d_core
                              fft_core detection_core dp_tlm_core cvt_core
                              ${JM_MATH_LIBRARY})
target_include_directories(test_search_equiv
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_search_equiv COMMAND test_search_equiv)

# The search flavors' C tests (#2752 step 3.4b), acq's until acq/ was
# deleted, registered here as the detectors' were in 3.3.
#
# test_correlator_search: acq's test_correlator_search. It builds a swept carrier
# with dp_swept_test.h, which is dp_wfm_synth_prod over a chirp synth: a
# TEST-ONLY dependency (the search never calls the generator), so it is on
# this target alone. The list is wfm_synth_core's closure, as
# test_wfm_synth_core links it; the cores are OBJECT libraries, and CMake
# adds an object library's objects only to a target that names it.
add_executable(test_correlator_search
               ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_correlator_search.c)
target_link_libraries(test_correlator_search
                      PRIVATE correlator_core Threads::Threads fft2d_core
                              fft_core detection_core dp_tlm_core cvt_core
                              wfm_synth_core lo_core awgn_core pn_core fir_core
                              resamp_core wfm_dsp_core wfm_frame_core gold_core
                              ${JM_MATH_LIBRARY})
target_include_directories(test_correlator_search
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_correlator_search COMMAND test_correlator_search)

# test_correlator_regrid_failure: a regrid that fails commits nothing
# (#2761). It compiles both correlator sources INTO itself, because a
# widening regrid is reachable only from the constructor, and renames the
# plan builder for that one translation unit so the Nth plan can be made to
# fail. The failable builder lives in its own file, which the rename is
# undone for, so it calls the real one.
add_executable(test_correlator_regrid_failure
    ${CMAKE_SOURCE_DIR}/native/tests/correlator/test_correlator_regrid_failure.c
    ${CMAKE_SOURCE_DIR}/native/tests/correlator/correlator_fft_hook.c)
target_compile_definitions(test_correlator_regrid_failure PRIVATE
    dp_fft_create=search_test_fft_create)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/native/tests/correlator/correlator_fft_hook.c
    PROPERTIES COMPILE_OPTIONS "-Udp_fft_create")
target_link_libraries(test_correlator_regrid_failure PRIVATE
    Threads::Threads fft2d_core fft_core detection_core
    dp_tlm_core cvt_core ${JM_MATH_LIBRARY})
target_include_directories(test_correlator_regrid_failure PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_correlator_regrid_failure
         COMMAND test_correlator_regrid_failure)
