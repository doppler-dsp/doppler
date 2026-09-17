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

add_library(acquire_core OBJECT acquire_core.c)
target_include_directories(acquire_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# acquire Python module — aggregates: CarrierAcquisition
Python3_add_library(acquire MODULE WITH_SOABI acquire_ext.c)
target_link_libraries(acquire PRIVATE
    acquire_core
    carrier_acq_core
    psd_core
    acc_trace_core
    detector_core
    detection_core
    spectral_core
    corr_core
    fft_core
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
