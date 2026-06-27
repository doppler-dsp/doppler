add_library(track_core OBJECT track_core.c)
target_include_directories(track_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)

if(BUILD_PYTHON)
# track Python module — aggregates: LoopFilter, Costas, Dll, Channel, SymbolSync, PartialDespreader
Python3_add_library(track MODULE WITH_SOABI track_ext.c)
target_link_libraries(track PRIVATE
    track_core
    loop_filter_core
    costas_core
    dll_core
    channel_core
    symsync_core
    pdespread_core
    lo_core
    nco_core
    farrow_core
    Python3::NumPy)
target_include_directories(track PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(track PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/track"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/track")
add_custom_command(TARGET track POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:track>"
        "${PYTHON_PACKAGE_DIR}/track/$<TARGET_FILE_NAME:track>"
    VERBATIM
    COMMENT "Copy track extension module")
endif()
