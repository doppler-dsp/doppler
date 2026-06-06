add_library(wfmgen_core OBJECT wfmgen_core.c bpsk_map.c qpsk_map.c wfm_awgn_amplitude.c wfm_ebno_to_snr_db.c)
target_include_directories(wfmgen_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# wfmgen Python module — aggregates: PN
Python3_add_library(wfmgen MODULE WITH_SOABI wfmgen_ext.c)
target_link_libraries(wfmgen PRIVATE
    wfmgen_core
    pn_core
    Python3::NumPy)
target_include_directories(wfmgen PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
if(WIN32 AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(wfmgen PRIVATE -static-libgcc)
    get_filename_component(gcc_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    foreach(dll_name IN ITEMS libwinpthread-1.dll)
        if(EXISTS "${gcc_bin}/${dll_name}")
            add_custom_command(TARGET wfmgen POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${gcc_bin}/${dll_name}"
                    "${PYTHON_PACKAGE_DIR}/wfmgen"
                VERBATIM
                COMMENT "Copy Windows runtime DLL ${dll_name}")
        endif()
    endforeach()
endif()
set_target_properties(wfmgen PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfmgen"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/wfmgen")
add_custom_command(TARGET wfmgen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:wfmgen>"
        "${PYTHON_PACKAGE_DIR}/wfmgen/$<TARGET_FILE_NAME:wfmgen>"
    VERBATIM
    COMMENT "Copy wfmgen extension module")
endif()
