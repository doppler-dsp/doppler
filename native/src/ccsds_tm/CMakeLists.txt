# ccsds_tm — hand-owned c_dep: CCSDS TM Synchronization and Channel Coding
# (131.0-B-3), which is a CONFIGURATION of the codes in `conv` and `rs` plus
# the parts that are the standard's own: the randomiser, the ASM, the dual
# basis and the interleaver. FEC is the general subject and lives in those two
# components; this one is a single document's picks.
#
# Its own c_dep rather than a lib inside wfmcompose because BOTH ends want it:
# wfmgen encodes, and frame/ber_meter will decode. Putting it in the composer
# would put the whole waveform-composition chain on a receiver's link line for
# the sake of a randomiser.
#
# Pure-C OBJECT libraries, no CPython. jm apply only emits the
# add_subdirectory for this dir (listed in [project].c_deps); this file is
# hand-maintained.

# ── the CCSDS transforms ────────────────────────────────────────────────────
# Split per stage rather than one file, because each stage is separately
# falsifiable against a published vector and a caller may want only one of
# them — a receiver derandomising a capture needs the randomiser and nothing
# else.
add_library(ccsds_tm_core OBJECT
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_tm/rand.c
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_tm/conv.c
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_tm/asm.c
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_tm/rs.c
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_tm/frame.c
    ${CMAKE_SOURCE_DIR}/native/src/ccsds_tm/desc.c)
target_include_directories(ccsds_tm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/native/inc)

# The CCSDS picks are a conv_code_t and an rs_code_t, and the assembler codes
# through conv_encode and rs_encode/rs_decode -- ccsds_tm holds the
# configurations, conv and rs own the arithmetic.
#
# wfm_frame_core is desc.c's: 131.0-B-3 section 9 expressed as a
# wfm_frame_desc_t. The dependency runs THIS way and must keep doing so --
# wfm/wfm_frame.h knows nothing about CCSDS, and if it called this
# component's kernels the two would form a cycle. That is why the kernels
# travel to the general assembler as a table (ccsds_tm_frame_ops) instead of
# being called from it.
# Threads::Threads because rs.c derives its field tables under
# `pthread_once` (gh-817). PUBLIC so every consumer -- the tests, the
# benches, and the .so -- picks up -pthread transitively, the same way
# wfm_compose_core carries it for dp_parallel.
target_link_libraries(ccsds_tm_core PUBLIC conv_core rs_core wfm_frame_core
                                           Threads::Threads)

# ── tests ───────────────────────────────────────────────────────────────────
add_executable(test_ccsds_tm_rand
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_tm_rand.c)
target_link_libraries(test_ccsds_tm_rand PRIVATE
    ccsds_tm_core conv_core rs_core)
target_include_directories(test_ccsds_tm_rand PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_tm_rand COMMAND test_ccsds_tm_rand)

add_executable(test_ccsds_tm_conv
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_tm_conv.c)
target_link_libraries(test_ccsds_tm_conv PRIVATE
    ccsds_tm_core conv_core rs_core)
target_include_directories(test_ccsds_tm_conv PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_tm_conv COMMAND test_ccsds_tm_conv)

add_executable(test_ccsds_tm_rs
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_tm_rs.c)
target_link_libraries(test_ccsds_tm_rs PRIVATE
    ccsds_tm_core conv_core rs_core)
target_include_directories(test_ccsds_tm_rs PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_tm_rs COMMAND test_ccsds_tm_rs)

# The first-call race (gh-817). A SEPARATE binary on purpose: the race is on
# the first call, so a process that has already encoded anything cannot
# reach it -- registering these cases inside test_ccsds_tm_rs would make
# them unable to fail.
add_executable(test_ccsds_tm_rs_race
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_tm_rs_race.c)
target_link_libraries(test_ccsds_tm_rs_race PRIVATE
    ccsds_tm_core conv_core rs_core Threads::Threads)
target_include_directories(test_ccsds_tm_rs_race PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_tm_rs_race COMMAND test_ccsds_tm_rs_race)

add_executable(test_ccsds_tm_asm
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_tm_asm.c)
target_link_libraries(test_ccsds_tm_asm PRIVATE
    ccsds_tm_core conv_core rs_core)
target_include_directories(test_ccsds_tm_asm PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_tm_asm COMMAND test_ccsds_tm_asm)

add_executable(test_ccsds_tm_frame
    ${CMAKE_SOURCE_DIR}/native/tests/test_ccsds_tm_frame.c)
# wfm_frame_core: the frame-description cross-check asserts that the general
# descriptor reproduces this component's own coverage table. The dependency
# points the way docs/design/frame-description.md has it -- CCSDS is a
# configuration of the general frame, not a peer of it.
target_link_libraries(test_ccsds_tm_frame PRIVATE
    ccsds_tm_core conv_core rs_core wfm_frame_core pn_core gold_core m)
target_include_directories(test_ccsds_tm_frame PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/tests)
add_test(NAME test_ccsds_tm_frame COMMAND test_ccsds_tm_frame)

# ── benchmark ───────────────────────────────────────────────────────────────
# Hand-registered for the same reason the tests above are: ccsds_tm is a
# c_dep, not one of jm's components, so `jm apply` writes no target and
# `jm bench` cannot run it (just-makeit#1023 -- run it by hand meanwhile).
# scripts/check_bench_coverage.py fails `make lint` if the target, the
# measurement, or the JSON name it writes under goes wrong.
add_executable(bench_ccsds_tm_core
    ${CMAKE_SOURCE_DIR}/native/benchmarks/bench_ccsds_tm_core.c)
target_link_libraries(bench_ccsds_tm_core PRIVATE
    ccsds_tm_core conv_core rs_core wfm_frame_core pn_core gold_core m)
target_include_directories(bench_ccsds_tm_core PRIVATE
    ${CMAKE_SOURCE_DIR}/native/inc
    ${CMAKE_SOURCE_DIR}/native/benchmarks)
