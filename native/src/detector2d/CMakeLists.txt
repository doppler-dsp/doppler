# OBJECT library — uses corr2d_core symbols; fft2d_core + fft_core resolved
# transitively.
add_library(detector2d_core OBJECT detector2d_core.c)
target_include_directories(detector2d_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/detector2d)
target_link_libraries(detector2d_core PUBLIC corr2d_core fft2d_core fft_core m)

add_executable(test_detector2d_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_detector2d_core.c)
target_include_directories(test_detector2d_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(test_detector2d_core
    PRIVATE detector2d_core corr2d_core fft2d_core fft_core m)
add_test(NAME test_detector2d_core COMMAND test_detector2d_core)
