# wfmgen_cli_test.cmake — drives the built `wfmgen` composer binary and checks
# its byte output. Invoked by ctest with -DEXE=<wfmgen> -DWAVEGEN=<wavegen>.
# Runs in the test's build directory; scratch files are written relative to it.

function(run)
    execute_process(COMMAND ${EXE} ${ARGN} RESULT_VARIABLE rc)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "wfmgen ${ARGN} exited ${rc}")
    endif()
endfunction()

function(expect_size path want)
    file(SIZE "${path}" got)
    if(NOT got EQUAL want)
        message(FATAL_ERROR "${path}: size ${got}, expected ${want}")
    endif()
endfunction()

function(expect_contains path needle)
    file(READ "${path}" body)
    string(FIND "${body}" "${needle}" pos)
    if(pos EQUAL -1)
        message(FATAL_ERROR "${path}: missing '${needle}'")
    endif()
endfunction()

# 1. raw cf32: 8 samples * 8 bytes = 64
run(--type tone --count 8 --sample_type cf32 -o wg_tone.bin)
expect_size(wg_tone.bin 64)

# 2. single-segment ≡ wavegen, byte-for-byte (same engine + flags)
run(--type qpsk --count 64 --sample_type ci16 --seed 7 -o wg_q.bin)
execute_process(COMMAND ${WAVEGEN} --type qpsk --count 64 --sample_type ci16
    --seed 7 -o wv_q.bin RESULT_VARIABLE wrc)
if(NOT wrc EQUAL 0)
    message(FATAL_ERROR "wavegen exited ${wrc}")
endif()
file(MD5 wg_q.bin h1)
file(MD5 wv_q.bin h2)
if(NOT h1 STREQUAL h2)
    message(FATAL_ERROR "wfmgen != wavegen for the same single-segment args")
endif()

# 3. BLUE type-1000: 512-byte header + 4*8 bytes, magic "BLUE"
run(--type tone --count 4 --sample_type cf32 --file_type blue -o wg.blue)
expect_size(wg.blue 544)
expect_contains(wg.blue "BLUE")

# 4. csv: text output, one line per sample
run(--type tone --freq 0 --count 3 --file_type csv -o wg.csv)
expect_contains(wg.csv ",")

# 5. --record: a wfmgen-1 spec naming the type
run(--type pn --count 16 --record wg_rec.json -o wg_pn.bin)
expect_contains(wg_rec.json "wfmgen-1")
expect_contains(wg_rec.json "pn")

# 6. --from-file round-trip: record a run, replay it, bytes identical
run(--type bpsk --count 50 --sps 4 --record wg_spec.json -o wg_direct.bin)
run(--from-file wg_spec.json -o wg_replay.bin)
file(MD5 wg_direct.bin d1)
file(MD5 wg_replay.bin d2)
if(NOT d1 STREQUAL d2)
    message(FATAL_ERROR "--from-file replay differs from the direct run")
endif()

# 7. SigMF: <base>.sigmf-data (raw) + <base>.sigmf-meta (json)
run(--type qpsk --count 8 --sample_type ci16 --file_type sigmf -o wg_cap)
expect_size(wg_cap.sigmf-data 32)  # 8 samples * ci16 (4 bytes/sample)
expect_contains(wg_cap.sigmf-meta "ci16_le")
expect_contains(wg_cap.sigmf-meta "qpsk")

message(STATUS "wfmgen_cli: OK")
