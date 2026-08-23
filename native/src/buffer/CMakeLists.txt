# C-level test — buffer/buffer.h is header-only (no _core.c to link). This is
# the only buffer gate on the macOS C job, which runs without BUILD_PYTHON, so
# it lives outside the guard below.
# dp_interrupt_obj: buffer.h is header-only, but its wait() now consults the
# interrupt flag, whose one definition lives in the core. These targets embed
# objects rather than linking libdoppler, so they need it named here.
add_executable(test_buffer_core
               ${CMAKE_SOURCE_DIR}/native/tests/test_buffer_core.c
               $<TARGET_OBJECTS:dp_interrupt_obj>)
# Threads: the end-of-stream ordering can only be exercised with a consumer
# already spinning while the producer writes and closes.
find_package(Threads REQUIRED)
target_link_libraries(test_buffer_core PRIVATE Threads::Threads)
target_include_directories(test_buffer_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
add_test(NAME test_buffer_core COMMAND test_buffer_core)

# Benchmark — same story as the test above: header-only, so there is no
# component core to link, and this CMakeLists is hand-owned (the module is
# no_generate), so the target is written here rather than by `jm apply`.
add_executable(bench_buffer_core
               ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_buffer_core.c
               $<TARGET_OBJECTS:dp_interrupt_obj>)
# libm because jm_bench.h's stats take a sqrt for the standard deviation.
# A Release build folds that call away and links without it; a Debug one
# (make test-tsan, make test-ubsan) does not, which is where this surfaced.
target_link_libraries(bench_buffer_core PRIVATE m)
target_include_directories(bench_buffer_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

if(BUILD_PYTHON)
# buffer Python module — wraps buffer/buffer.h (header-only, no C source link)
# dp_interrupt_obj: buffer.h's wait() consults the interrupt flag, and this
# extension embeds the header rather than linking libdoppler.
Python3_add_library(buffer MODULE WITH_SOABI buffer_ext.c
    $<TARGET_OBJECTS:dp_interrupt_obj>)
target_link_libraries(buffer PRIVATE Python3::NumPy)
target_include_directories(buffer PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(buffer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/buffer"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/buffer")
add_custom_command(TARGET buffer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:buffer>"
        "${PYTHON_PACKAGE_DIR}/buffer/$<TARGET_FILE_NAME:buffer>"
    VERBATIM)
endif()
