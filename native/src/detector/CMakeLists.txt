# OBJECT library — uses corr_core symbols; fft_core resolved transitively.
add_library(detector_core OBJECT detector_core.c)
target_include_directories(detector_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/inc/detector)
target_link_libraries(detector_core PUBLIC corr_core fft_core m)

add_executable(test_detector_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_detector_core.c)
target_include_directories(test_detector_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(test_detector_core
    PRIVATE detector_core corr_core fft_core m)
add_test(NAME test_detector_core COMMAND test_detector_core)
