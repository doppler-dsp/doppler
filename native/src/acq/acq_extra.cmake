# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# test_acq_core builds a swept carrier with dp_swept_test.h, which is
# dp_wfm_synth_prod over a chirp synth. That is a TEST-ONLY dependency (the
# engine never calls the generator), so it is added here to the test target
# alone: a `depends_on` entry would put the generator on acq_core's own link
# line. The list is wfm_synth_core's closure, as test_wfm_synth_core links it,
# and naming wfm_synth_core alone does not link. The cores are OBJECT
# libraries, and CMake adds an object library's objects only to a target that
# names it: wfm_synth_core's PUBLIC dependencies pass on their usage
# requirements but not their objects.
target_link_libraries(test_acq_core PRIVATE
    wfm_synth_core lo_core awgn_core pn_core fir_core resamp_core
    wfm_dsp_core wfm_frame_core cvt_core gold_core)

# A regrid that fails must commit nothing (#2761). The test compiles
# acq_core.c INTO itself, because acq_regrid is static (a widening regrid is
# reachable only from the constructor, where a failure ends the engine), and
# renames the plan builder for that one translation unit so the Nth plan
# acq_core.c and corr2d_core.c (the map acq builds, compiled into the test, so
# corr2d_core is not linked) build can be made to fail. The failable builder lives in its own
# file, which the rename is undone for, so it calls the real one.
add_executable(test_acq_regrid_failure
    ${CMAKE_SOURCE_DIR}/native/tests/acq/test_acq_regrid_failure.c
    ${CMAKE_SOURCE_DIR}/native/tests/acq/acq_fft_hook.c)
target_compile_definitions(test_acq_regrid_failure PRIVATE
    dp_fft_create=acq_test_fft_create)
set_source_files_properties(
    ${CMAKE_SOURCE_DIR}/native/tests/acq/acq_fft_hook.c
    PROPERTIES COMPILE_OPTIONS "-Udp_fft_create")
target_link_libraries(test_acq_regrid_failure PRIVATE
    Threads::Threads fft2d_core fft_core detection_core
    dp_tlm_core cvt_core ${JM_MATH_LIBRARY})
target_include_directories(test_acq_regrid_failure PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc ${CMAKE_SOURCE_DIR}/native/inc/doppler/acq
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_acq_regrid_failure COMMAND test_acq_regrid_failure)
