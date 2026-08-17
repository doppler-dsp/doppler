# conv — hand-owned c_dep: convolutional codes, both directions.
#
# Its own c_dep rather than a library inside `fec`, because the code family is
# not CCSDS's: `fec` holds a CONFIGURATION of it (FEC_CCSDS_CONV) the way any
# other caller would. A decoder that lived under `fec` would put the whole
# channel-coding chain on the link line of anyone who wanted a trellis.
#
# Pure-C OBJECT library, no CPython. jm apply only emits the add_subdirectory
# for this dir (listed in [project].c_deps); this file is hand-maintained.

add_library(conv_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/conv/conv_core.c)
target_include_directories(conv_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

add_executable(test_conv_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_conv_core.c)
# No fec: the test's external truth is the impulse response, which is what a
# generator polynomial means and is checkable for every configuration. Holding
# the CCSDS numbers to the standard is test_fec_ccsds_conv.c's job.
target_link_libraries(test_conv_core PRIVATE conv_core m)
target_include_directories(test_conv_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_conv_core COMMAND test_conv_core)
