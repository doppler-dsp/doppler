# telemetry — hand-owned (no_generate) module: lightweight scalar telemetry
# taps (telemetry/telemetry.h + telemetry_core.c).  Pure C99, no deps beyond
# the header-only SPSC ring (buffer/buffer.h), so the core rides in
# doppler_lib unconditionally — the detached cost is one branch per event.

# OBJECT library — pure C core, no Python dependency.
add_library(telemetry_core OBJECT telemetry_core.c tlm_capture.c)
target_include_directories(telemetry_core
                           PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)
# A file-backed capture writes on its own thread so disk latency never lands
# on the block boundary (dp_parallel.h precedent).
target_link_libraries(telemetry_core PUBLIC Threads::Threads)

# Fold into the combined lib from here (not the root CMakeLists — the root's
# component region is jm-managed, and telemetry is no_generate/hand-owned).
target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:telemetry_core>)
target_sources(doppler_lib_static PRIVATE $<TARGET_OBJECTS:telemetry_core>)

# C-level test — also the only telemetry gate on the macOS C job, which runs
# without BUILD_PYTHON, so it lives outside any guard.
if(BUILD_PYTHON)
# telemetry Python module — hand-written ext over telemetry_core (numpy
# structured-array read; capsule attach point for instrumented objects).
Python3_add_library(telemetry MODULE WITH_SOABI telemetry_ext.c)
target_link_libraries(telemetry PRIVATE telemetry_core Python3::NumPy)
target_include_directories(telemetry PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc)
set_target_properties(telemetry PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/telemetry"
    RUNTIME_OUTPUT_DIRECTORY "${PYTHON_PACKAGE_DIR}/telemetry")
add_custom_command(TARGET telemetry POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:telemetry>"
        "${PYTHON_PACKAGE_DIR}/telemetry/$<TARGET_FILE_NAME:telemetry>"
    VERBATIM)
endif()

add_executable(bench_telemetry_core
               ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_telemetry_core.c)
target_link_libraries(bench_telemetry_core PRIVATE telemetry_core
                                                   Threads::Threads)
target_include_directories(bench_telemetry_core
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/benchmarks)

foreach(t test_telemetry_core test_tlm_capture)
  add_executable(${t} ${CMAKE_SOURCE_DIR}/native/tests/${t}.c)
  target_link_libraries(${t} PRIVATE telemetry_core Threads::Threads)
  target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
