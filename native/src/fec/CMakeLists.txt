# fec — hand-owned c_dep: CCSDS TM channel coding.
#
# Its own c_dep rather than a lib inside wfmcompose because BOTH ends want it:
# wfmgen encodes, and frame/ber_meter will decode. Putting it in the composer
# would put the whole waveform-composition chain on a receiver's link line for
# the sake of a randomiser.
#
# Pure-C OBJECT libraries, no CPython. jm apply only emits the
# add_subdirectory for this dir (listed in [project].c_deps); this file is
# hand-maintained.

# ── the CCSDS transforms ────────────────────────────────────────────────────
# Split per stage rather than one fec_core, because each stage is separately
# falsifiable against a published vector and a caller may want only one of
# them — a receiver derandomising a capture needs the randomiser and nothing
# else.
add_library(fec_ccsds_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/fec/ccsds_rand.c)
target_include_directories(fec_ccsds_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc)

# ── tests ───────────────────────────────────────────────────────────────────
add_executable(test_fec_ccsds_rand
    ${CMAKE_SOURCE_DIR}/native/tests/test_fec_ccsds_rand.c)
target_link_libraries(test_fec_ccsds_rand PRIVATE fec_ccsds_core)
target_include_directories(test_fec_ccsds_rand PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_fec_ccsds_rand COMMAND test_fec_ccsds_rand)
