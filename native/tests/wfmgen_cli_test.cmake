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

# Assert a specific exit code (for the usage-error / crash-regression cases).
function(expect_exit code)
    execute_process(COMMAND ${EXE} ${ARGN}
                    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
    if(NOT rc EQUAL ${code})
        message(FATAL_ERROR "wfmgen ${ARGN}: exit ${rc}, expected ${code}")
    endif()
endfunction()

# 1. raw cf32: 8 samples * 8 bytes = 64
run(--type tone --count 8 --sample-type cf32 -o wg_tone.bin)
expect_size(wg_tone.bin 64)

# 2. single-segment output is byte-stable, frozen as an MD5 golden — the
#    regression anchor for the single-segment path. Regenerated when the
#    friendly CLI defaults landed (fs=1.0, sps=1, seed=0): this run omits --sps,
#    so the new sps=1 default (1 sample/symbol, not 8) moved the bytes.
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

# 10. Usage errors exit 2 — and a value-taking flag with no value must be a
#     clean usage error, NOT a segfault (regression for the strtod(NULL) crash).
expect_exit(2 --type tone --count 4 --freq)        # missing value (was SIGSEGV)
expect_exit(2 --fs)                                 # missing value, flag is last
expect_exit(2 --nope)                               # unknown option
expect_exit(2 --type bpsk --pulse rrc --rrc-beta 5 --count 4 -o -)  # beta > 1

# 11. --version prints the doppler banner and exits 0.
execute_process(COMMAND ${EXE} --version
    OUTPUT_VARIABLE ver_out RESULT_VARIABLE ver_rc)
if(NOT ver_rc EQUAL 0)
    message(FATAL_ERROR "wfmgen --version exited ${ver_rc}")
endif()
string(FIND "${ver_out}" "wfmgen (doppler)" ver_pos)
if(ver_pos EQUAL -1)
    message(FATAL_ERROR "wfmgen --version banner missing: ${ver_out}")
endif()

# 12. symbols round-trip: a cf32 file fed back as --type symbols at sps=1 with
#     no carrier reproduces the input samples byte-for-byte (the symbol IS the
#     sample). Proves the --symbols-file read + composer wiring end-to-end.
run(--type qpsk --sps 1 --count 6 --sample-type cf32 --seed 3 -o wg_syms_in.cf32)
run(--type symbols --symbols-file wg_syms_in.cf32 --sps 1 --count 6
    --sample-type cf32 -o wg_syms_out.cf32)
file(MD5 wg_syms_in.cf32 si)
file(MD5 wg_syms_out.cf32 so)
if(NOT si STREQUAL so)
    message(FATAL_ERROR "symbols sps=1 round-trip differs from the input cf32")
endif()

# 13. symbols missing flag value is a clean usage error (exit 2), not a crash.
#     (A streamless symbols synth emits zeros, like a pattern-less bits synth.)
expect_exit(2 --type symbols --symbols-file)        # missing value

message(STATUS "wfmgen_cli: OK")
