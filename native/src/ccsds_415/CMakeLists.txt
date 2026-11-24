# ccsds_415 — hand-owned c_dep: the CCSDS 415.1-B-1 specification point the
# async DSSS receiver is specified for (doppler/ccsds/ccsds_415.h): its
# numbers, the chip rate, C/N0 and a scene's derived rates.
#
# Its own c_dep rather than a function file in the `ccsds` module, for the
# reason ccsds_tm is one: the receiver and the pool build at the point, and
# the ccsds module's object library would put the TM coding chain (its
# asm_bits needs ccsds_tm, conv, rs, wfm_frame) on a receiver's link line.
# The `ccsds` module's ccsds415_* functions are thin aliases over this.
#
# Pure C, no CPython. jm apply only emits the add_subdirectory for this dir
# (listed in [project].c_deps); this file is hand-maintained.
add_library(ccsds_415_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_415/ccsds_415.c)
target_include_directories(ccsds_415_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc)
find_library(CCSDS_415_MATH_LIBRARY m)
if(CCSDS_415_MATH_LIBRARY)
  target_link_libraries(ccsds_415_core PUBLIC ${CCSDS_415_MATH_LIBRARY})
endif()

# One period of the point's stimulus as a frame description
# (ccsds_415_frame.h). Its own OBJECT library because it links the frame
# layer (wfm_frame_core, and through it pn, gold and cvt), and the receiver
# views link ccsds_415_core for the arithmetic alone.
add_library(ccsds_415_frame_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_415/ccsds_415_frame.c)
target_include_directories(ccsds_415_frame_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(ccsds_415_frame_core PUBLIC ccsds_415_core
                                                  wfm_frame_core)

# ── tests ───────────────────────────────────────────────────────────────────
add_executable(test_ccsds_415
    ${CMAKE_SOURCE_DIR}/native/tests/ccsds_415/test_ccsds_415.c)
target_link_libraries(test_ccsds_415 PRIVATE ccsds_415_core)
target_include_directories(test_ccsds_415 PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_415 COMMAND test_ccsds_415)

add_executable(test_ccsds_415_frame
    ${CMAKE_SOURCE_DIR}/native/tests/ccsds_415/test_ccsds_415_frame.c)
target_link_libraries(test_ccsds_415_frame PRIVATE ccsds_415_frame_core
    ccsds_415_core wfm_frame_core pn_core gold_core cvt_core)
target_include_directories(test_ccsds_415_frame PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_415_frame COMMAND test_ccsds_415_frame)
