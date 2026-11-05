# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# test_corr_equiv: the 1-D correlator against the 2-D one at one row, byte
# for byte (#2752 step 3.2, docs/design/correlator.md section 2.1). It links
# both cores, so it lives beside neither object's generated test.
add_executable(test_corr_equiv
               ${CMAKE_SOURCE_DIR}/native/tests/corr2d/test_corr_equiv.c)
target_link_libraries(test_corr_equiv
                      PRIVATE corr_core corr2d_core Threads::Threads
                              fft2d_core fft_core ${JM_MATH_LIBRARY})
target_include_directories(test_corr_equiv
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_corr_equiv COMMAND test_corr_equiv)
