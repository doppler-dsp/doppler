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

add_library(ber_core OBJECT ber_core.c ber_theory_ser.c ber_theory_ber.c ber_esn0_db_for_ser.c ber_evm_scatter_floor_db.c ber_settle_syms.c ber_lock_symbol.c ber_evm_db.c ber_settle_from.c)
target_include_directories(ber_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# ber Python module — aggregates: BerMeter, FrameMeter
Python3_add_library(ber MODULE WITH_SOABI ber_ext.c)
target_link_libraries(ber PRIVATE
    ber_core
    ber_meter_core
    frame_meter_core
    detection_core
    Python3::NumPy)
target_include_directories(ber PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(ber PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ber"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/ber")
add_custom_command(TARGET ber POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:ber>"
        "${PYTHON_PACKAGE_DIR}/ber/$<TARGET_FILE_NAME:ber>"
    VERBATIM
    COMMENT "Copy ber extension module")
endif()
