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

add_library(coding_core OBJECT coding_core.c)
target_include_directories(coding_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# coding Python module — aggregates: ConvEncoder, Viterbi, ReedSolomon, Interleaver, Deinterleaver
Python3_add_library(coding MODULE WITH_SOABI coding_ext.c)
target_link_libraries(coding PRIVATE
    coding_core
    conv_enc_core
    viterbi_core
    rs_codec_core
    interleaver_core
    conv_core
    rs_core
    Python3::NumPy)
target_include_directories(coding PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(coding PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/coding"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/coding")
add_custom_command(TARGET coding POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:coding>"
        "${PYTHON_PACKAGE_DIR}/coding/$<TARGET_FILE_NAME:coding>"
    VERBATIM
    COMMENT "Copy coding extension module")
endif()
