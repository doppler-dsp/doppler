add_library(util_core OBJECT util_core.c square_clip.c)
target_include_directories(util_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# util Python module
Python3_add_library(util MODULE WITH_SOABI util_ext.c)
target_link_libraries(util PRIVATE
    util_core
    Python3::NumPy)
target_include_directories(util PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(util PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/util"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/util")
add_custom_command(TARGET util POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:util>"
        "${PYTHON_PACKAGE_DIR}/util/$<TARGET_FILE_NAME:util>"
    VERBATIM
    COMMENT "Copy util extension module")
endif()
