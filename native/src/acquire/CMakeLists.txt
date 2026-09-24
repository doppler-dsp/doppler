# gh-1305: resolve libm to a PATH, not the bare name `m`. A bare `m` in
# `target_link_libraries` is looked up as a CMake TARGET first, so a module
# named `m` (`jm module m`) shadows the system math library: the object's
# test/bench link the module's DSO and fail with `undefined reference to sqrt`,
# and the module's own pair fail configure outright with "Target "m" of type
# MODULE_LIBRARY may not be linked into another target". A `find_library`
# result is an absolute path that no target name can shadow. Empty where libm
# lives in libc (MSVC, and platforms that fold it into libSystem), which links
# nothing.
find_library(JM_MATH_LIBRARY m)
if(NOT JM_MATH_LIBRARY)
  set(JM_MATH_LIBRARY "")
endif()

add_library(acquire_core OBJECT acquire_core.c bin_to_signed.c)
target_include_directories(acquire_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# acquire Python module — aggregates: CarrierAcquisition, Acquisition, BurstAcquisition, BurstCapture, PersistentBurstCapture
Python3_add_library(acquire MODULE WITH_SOABI acquire_ext.c)
target_link_libraries(acquire PRIVATE
    acquire_core
    carrier_acq_core
    acq_core
    burst_acq_core
    burst_capture_core
    psd_core
    acc_trace_core
    detector_core
    detection_core
    spectral_core
    corr_core
    fft_core
    Threads::Threads
    corr2d_core
    fft2d_core
    dp_tlm_core
    Python3::NumPy)
target_include_directories(acquire PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(acquire PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/acquire"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/acquire")
add_custom_command(TARGET acquire POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:acquire>"
        "${PYTHON_PACKAGE_DIR}/acquire/$<TARGET_FILE_NAME:acquire>"
    VERBATIM
    COMMENT "Copy acquire extension module")
endif()

add_executable(test_acquire_core
    ${CMAKE_SOURCE_DIR}/native/tests/test_acquire_core.c)
target_link_libraries(test_acquire_core PRIVATE
    acquire_core
    carrier_acq_core
    acq_core
    burst_acq_core
    burst_capture_core
    psd_core
    acc_trace_core
    detector_core
    detection_core
    spectral_core
    corr_core
    fft_core
    Threads::Threads
    corr2d_core
    fft2d_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(test_acquire_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_acquire_core COMMAND test_acquire_core)

add_executable(bench_acquire_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_acquire_core.c)
target_link_libraries(bench_acquire_core PRIVATE
    acquire_core
    carrier_acq_core
    acq_core
    burst_acq_core
    burst_capture_core
    psd_core
    acc_trace_core
    detector_core
    detection_core
    spectral_core
    corr_core
    fft_core
    Threads::Threads
    corr2d_core
    fft2d_core
    dp_tlm_core
    ${JM_MATH_LIBRARY})
target_include_directories(bench_acquire_core
    PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
            ${CMAKE_SOURCE_DIR}/native/benchmarks)

# Your own CMake for this directory goes in acquire_extra.cmake beside this
# file: jm includes it and never writes it (gh-1351). This file is
# regenerated.
include(${CMAKE_CURRENT_LIST_DIR}/acquire_extra.cmake OPTIONAL)
