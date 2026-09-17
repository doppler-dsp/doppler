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

add_library(snr_core OBJECT snr_core.c snr_data_aided_db.c snr_m2m4_db.c snr_data_aided_db_series.c snr_m2m4_db_series.c)
target_include_directories(snr_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# snr Python module
Python3_add_library(snr MODULE WITH_SOABI snr_ext.c)
target_link_libraries(snr PRIVATE
    snr_core
    Python3::NumPy)
target_include_directories(snr PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(snr PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/snr"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/snr")
add_custom_command(TARGET snr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:snr>"
        "${PYTHON_PACKAGE_DIR}/snr/$<TARGET_FILE_NAME:snr>"
    VERBATIM
    COMMENT "Copy snr extension module")
endif()
