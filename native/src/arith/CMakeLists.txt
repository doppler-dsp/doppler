add_library(arith_core OBJECT arith_core.c add_q15.c sub_q15.c mul_q15.c dot_q15.c shl_q15.c shr_q15.c add_q8.c sub_q8.c mul_q8.c dot_q8.c shl_q8.c shr_q8.c shl_i64.c shr_i64.c)
target_include_directories(arith_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# arith Python module — aggregates: AccQ15, AccQ8
Python3_add_library(arith MODULE WITH_SOABI arith_ext.c)
target_link_libraries(arith PRIVATE
    arith_core
    acc_q15_core
    acc_q8_core
    Python3::NumPy)
target_include_directories(arith PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(arith PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/arith"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/arith")
add_custom_command(TARGET arith POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:arith>"
        "${PYTHON_PACKAGE_DIR}/arith/$<TARGET_FILE_NAME:arith>"
    VERBATIM
    COMMENT "Copy arith extension module")
endif()
