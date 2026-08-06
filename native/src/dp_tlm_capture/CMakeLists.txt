# dp_tlm_capture — hand-owned: the lossless boundary drain over a dp_tlm ring.
#
# Not jm-generated yet. It will become a declarative object once
# just-buildit/just-makeit#790's capsule init_param and #794's handle capsule
# are both wired here — its constructor takes a dp_tlm_t* AND a
# dp_sample_clock_t*, one from an object and one from a handle.
add_library(dp_tlm_capture_core OBJECT dp_tlm_capture_core.c)
target_include_directories(dp_tlm_capture_core
                           PUBLIC ${CMAKE_SOURCE_DIR}/native/inc)
# A file-backed capture writes on its own thread so disk latency never lands
# on the block boundary (dp_parallel.h precedent).
target_link_libraries(dp_tlm_capture_core PUBLIC Threads::Threads)

target_sources(doppler_lib PRIVATE $<TARGET_OBJECTS:dp_tlm_capture_core>)
target_sources(doppler_lib_static
               PRIVATE $<TARGET_OBJECTS:dp_tlm_capture_core>)

# C gates. These also run on the macOS C job, which builds without
# BUILD_PYTHON, so they live outside any guard.
foreach(t test_tlm_capture test_tlm_bound_real test_telemetry_core)
  add_executable(${t} ${CMAKE_SOURCE_DIR}/native/tests/${t}.c)
  target_include_directories(${t} PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
  add_test(NAME ${t} COMMAND ${t})
endforeach()
target_link_libraries(test_tlm_capture
                      PRIVATE dp_tlm_capture_core dp_tlm_core timing_core
                              Threads::Threads)
# The hand-written dp_tlm suite (block_bound / avail / resize / stats / the
# SPSC thread hand-off). jm scaffolds its own test_dp_tlm_core; this is the
# real one and must stay registered alongside it.
target_link_libraries(test_telemetry_core
                      PRIVATE dp_tlm_capture_core dp_tlm_core Threads::Threads)
# The bound is claimed from a code audit of every emit site; this drives a REAL
# object (carrier_nda -- the only unconditional per-input emitter) and measures
# what it actually emits.
target_link_libraries(test_tlm_bound_real
                      PRIVATE dp_tlm_capture_core dp_tlm_core carrier_nda_core
                              lo_core loop_filter_core boxcar_core agc_core
                              lockdet_core Threads::Threads m)
