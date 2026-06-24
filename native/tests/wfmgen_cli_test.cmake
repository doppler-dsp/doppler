# wfmgen_cli_test.cmake — drives the built `wfmgen` composer binary and checks
# its byte output. Invoked by ctest with -DEXE=<wfmgen>.
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
run(--type tone --count 8 --sample-type cf32 -o wg_tone.bin)
expect_size(wg_tone.bin 64)

# 2. single-segment output is byte-stable, frozen as an MD5 golden — the
#    regression anchor for the single-segment path. Regenerated when the
#    friendly CLI defaults landed (fs=1.0, sps=1, headroom=3): this run omits
#    --sps/--headroom, so the new sps=1 + 3 dB backoff moved the bytes.
run(--type qpsk --count 64 --sample-type ci16 --seed 7 -o wg_q.bin)
file(MD5 wg_q.bin h1)
set(WG_Q_GOLDEN "d0afb7878f1e0eb189183dcca28610f8")
if(NOT h1 STREQUAL WG_Q_GOLDEN)
    message(FATAL_ERROR
        "wfmgen single-segment output drifted: got ${h1}, want ${WG_Q_GOLDEN}")
endif()

# 3. BLUE type-1000: 512-byte header + 4*8 bytes, magic "BLUE"
run(--type tone --count 4 --sample-type cf32 --file-type blue -o wg.blue)
expect_size(wg.blue 544)
expect_contains(wg.blue "BLUE")

# 4. csv: text output, one line per sample
run(--type tone --freq 0 --count 3 --file-type csv -o wg.csv)
expect_contains(wg.csv ",")

# 5. --record: version is integer 1, spec names the type
run(--type pn --count 16 --record wg_rec.json -o wg_pn.bin)
expect_contains(wg_rec.json "\"version\"")
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
run(--type qpsk --count 8 --sample-type ci16 --file-type sigmf -o wg_cap)
expect_size(wg_cap.sigmf-data 32)  # 8 samples * ci16 (4 bytes/sample)
expect_contains(wg_cap.sigmf-meta "ci16_le")
expect_contains(wg_cap.sigmf-meta "qpsk")

# 8. Fibonacci LFSR differs from Galois
run(--type pn --pn-length 7 --sps 1 --count 127 --lfsr galois    -o wg_g.bin)
run(--type pn --pn-length 7 --sps 1 --count 127 --lfsr fibonacci -o wg_f.bin)
file(MD5 wg_g.bin h_g)
file(MD5 wg_f.bin h_f)
if(h_g STREQUAL h_f)
    message(FATAL_ERROR "galois and fibonacci produced identical output")
endif()

# 9. BLUE detached: <base>.hdr (512-byte HCB) + <base>.det (raw data)
run(--type tone --count 8 --sample-type cf32 --file-type blue --detached -o wg_det)
expect_size(wg_det.hdr 512)         # header only
expect_size(wg_det.det 64)          # 8 * cf32 (8 bytes/sample), no header
expect_contains(wg_det.hdr "BLUE")

message(STATUS "wfmgen_cli: OK")
