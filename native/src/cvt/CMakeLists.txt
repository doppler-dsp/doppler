add_library(cvt_core OBJECT cvt_core.c)
target_include_directories(cvt_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# cvt Python module — aggregates: F32ToI16, I16ToF32, F32ToI16U32, F32ToI16U64, I16U32ToF32, I16U64ToF32, F32ToUQ15, UQ15ToF32
Python3_add_library(cvt MODULE WITH_SOABI cvt_ext.c)
target_link_libraries(cvt PRIVATE
    cvt_core
    f32_to_i16_core
    i16_to_f32_core
    f32_to_i16u32_core
    f32_to_i16u64_core
    i16u32_to_f32_core
    i16u64_to_f32_core
    f32_to_uq15_core
    uq15_to_f32_core
    Python3::NumPy)
target_include_directories(cvt PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(cvt PRIVATE -static-libgcc)
    get_filename_component(gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(dll_name IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${gcc_bin}/${dll_name}")
            add_custom_command(TARGET cvt POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${gcc_bin}/${dll_name}"
                    "${PYTHON_PACKAGE_DIR}/cvt"
                VERBATIM
                COMMENT "Copy Windows runtime DLL ${dll_name}")
        endif()
    endforeach()
endif()
set_target_properties(cvt PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/cvt"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/cvt")
add_custom_command(TARGET cvt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:cvt>"
        "${PYTHON_PACKAGE_DIR}/cvt/$<TARGET_FILE_NAME:cvt>"
    VERBATIM
    COMMENT "Copy cvt extension module")
endif()
