# OBJECT library — pure C core, no Python dependency.
add_library(delay_core OBJECT delay_core.c)
target_include_directories(
  delay_core PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                            ${CMAKE_SOURCE_DIR}/native/inc/delay)

add_executable(test_delay_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_delay_core.c)
target_link_libraries(test_delay_core
                      PRIVATE delay_core m)
target_include_directories(test_delay_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_delay_core COMMAND test_delay_core)

add_executable(
  bench_delay_core
  ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_delay_core.c)
target_link_libraries(bench_delay_core
                      PRIVATE delay_core m)
target_include_directories(
  bench_delay_core PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)
if(BUILD_PYTHON)
# delay Python module — aggregates: DelayCf64
Python3_add_library(delay MODULE WITH_SOABI delay_ext.c)
target_link_libraries(delay PRIVATE
    delay_core
    Python3::NumPy)
target_include_directories(delay PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(delay PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/delay"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/delay")
add_custom_command(TARGET delay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:delay>"
        "${PYTHON_PACKAGE_DIR}/delay/$<TARGET_FILE_NAME:delay>"
    VERBATIM
    COMMENT "Copy delay extension module")
endif()
