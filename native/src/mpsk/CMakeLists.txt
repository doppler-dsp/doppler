add_library(mpsk_core OBJECT mpsk_core.c mpsk_map.c mpsk_demap.c mpsk_diff_map.c mpsk_diff_demap.c mpsk_bits_per_symbol.c)
target_include_directories(mpsk_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# mpsk Python module
Python3_add_library(mpsk MODULE WITH_SOABI mpsk_ext.c)
target_link_libraries(mpsk PRIVATE
    mpsk_core
    Python3::NumPy)
target_include_directories(mpsk PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(mpsk PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/mpsk"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/mpsk")
add_custom_command(TARGET mpsk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mpsk>"
        "${PYTHON_PACKAGE_DIR}/mpsk/$<TARGET_FILE_NAME:mpsk>"
    VERBATIM
    COMMENT "Copy mpsk extension module")
endif()
