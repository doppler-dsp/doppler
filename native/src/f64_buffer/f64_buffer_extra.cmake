# TEMPORARY (just-buildit/just-makeit#1432): see buffer_extra.cmake.
foreach(_t test_f64_buffer_core bench_f64_buffer_core)
  if(TARGET ${_t})
    target_sources(${_t} PRIVATE $<TARGET_OBJECTS:dp_interrupt_obj>)
  endif()
endforeach()
