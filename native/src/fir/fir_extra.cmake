# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# test_fir_chunk: dp_fir is chunk-invariant (#1893). The defect it pins lived
# only in the VECTOR paths of fir_core.c -- the tail that finishes a block
# rounded `h * x` before adding, the vector body fused it -- so a test linked
# against the portable build (no FMA, no wide vector) passes with or without
# the fault. Each instruction set the kernel has a path for therefore gets
# its own copy of the kernel and its own test; one the host CPU lacks reports
# SKIPPED (exit 77), not passed.
#
#   test_fir_chunk          the build's own flags (NEON on aarch64)
#   test_fir_chunk_avx2     x86-64, -mavx2 -mfma
#   test_fir_chunk_avx512   x86-64, -mavx512f -mavx512dq (+ the above)
add_executable(test_fir_chunk
               ${CMAKE_SOURCE_DIR}/native/tests/test_fir_chunk.c)
target_link_libraries(test_fir_chunk PRIVATE fir_core ${JM_MATH_LIBRARY})
target_include_directories(test_fir_chunk
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_fir_chunk COMMAND test_fir_chunk)
set_tests_properties(test_fir_chunk PROPERTIES SKIP_RETURN_CODE 77)

if(NOT MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
  foreach(_isa avx2 avx512)
    if(_isa STREQUAL "avx2")
      set(_flags -mavx2 -mfma)
      set(_def FIR_REQUIRE_AVX2)
    else()
      set(_flags -mavx2 -mfma -mavx512f -mavx512dq)
      set(_def FIR_REQUIRE_AVX512)
    endif()
    add_library(fir_core_${_isa} OBJECT
                ${CMAKE_CURRENT_LIST_DIR}/fir_core.c)
    target_include_directories(
      fir_core_${_isa} PUBLIC ${CMAKE_SOURCE_DIR}/native/inc
                              ${CMAKE_SOURCE_DIR}/native/inc/doppler/fir)
    dp_gnu_compile_options(fir_core_${_isa} PRIVATE ${_flags})
    add_executable(test_fir_chunk_${_isa}
                   ${CMAKE_SOURCE_DIR}/native/tests/test_fir_chunk.c)
    target_compile_definitions(test_fir_chunk_${_isa} PRIVATE ${_def})
    target_link_libraries(test_fir_chunk_${_isa}
                          PRIVATE fir_core_${_isa} ${JM_MATH_LIBRARY})
    target_include_directories(test_fir_chunk_${_isa}
                               PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                       ${CMAKE_SOURCE_DIR}/native/tests)
    add_test(NAME test_fir_chunk_${_isa} COMMAND test_fir_chunk_${_isa})
    set_tests_properties(test_fir_chunk_${_isa} PROPERTIES SKIP_RETURN_CODE 77)
  endforeach()
endif()
