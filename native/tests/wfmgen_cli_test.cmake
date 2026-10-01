# wfmgen_cli_test.cmake — drives the built `wfmgen` composer binary and checks
# its byte output. Invoked by ctest with -DEXE=<wfmgen>.
# Runs in the test's build directory; scratch files are written relative to it,
# all named wg_* or wg.*. They are swept before the first case, so a stale file
# from an earlier run cannot satisfy a check, and again after the last one
# passes -- a failing run keeps them for inspection. `make test` fails on any
# file a passing test leaves behind (scripts/check_test_leaks.py).

# A `cmake -P` script sets no policies of its own; this is the project's floor.
cmake_minimum_required(VERSION 3.16)

function(sweep_scratch)
    file(GLOB _wg LIST_DIRECTORIES false "wg[._]*")
    if(_wg)
        file(REMOVE ${_wg})
    endif()
endfunction()

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

sweep_scratch()

# 1. raw cf32: 8 samples * 8 bytes = 64
run(--type tone --count 8 --sample-type cf32 -o wg_tone.bin)
expect_size(wg_tone.bin 64)

# 2. single-segment output is byte-stable, frozen as an MD5 golden — the
#    regression anchor for the single-segment path. Regenerated when the
#    friendly CLI defaults landed (fs=1.0, sps=1, seed=0): this run omits --sps,
#    so the new sps=1 default (1 sample/symbol, not 8) moved the bytes.
#
#    Regenerated again for doppler#1117: the writer quantised with a private
#    copy that TRUNCATED at a full scale of 32767, and now routes through the
#    cvt converters, which round to nearest at 2^15. Every one of the 128
#    codes moved, each by at most 1 LSB. The hash below was not merely
#    re-recorded -- the new bytes were checked to be bit-identical to
#    `doppler.cvt.F32ToI16().steps()` over the same run's cf32 output, which
#    is the property the issue asks for, and that check is kept as a test
#    (src/doppler/wfm/tests/test_wire_matches_cvt.py) so the next drift is
#    caught by an oracle rather than by an unexplained hash change.
run(--type qpsk --count 64 --sample-type ci16 --seed 7 -o wg_q.bin)
file(MD5 wg_q.bin h1)
set(WG_Q_GOLDEN "295c3dc1f142460f3c3699d8298acb16")
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

# 14. Continuous async DSSS (--symbol-rate): a finite --count generates, and a
#     --record → --from-file replay is byte-identical, for each data source
#     (default PRBS and --code-only). The data code is a 0/1 string;
#     symbol_rate independent of the chip clock is the asynchronicity.
set(DC "1111100110101001000101111")   # 25-chip data code (arbitrary)
run(--type dsss --data-code ${DC} --symbol-rate 2700 --sps 2 --fs 6138000
    --count 4096 --record wg_cont.json -o wg_cont_a.cf32)
run(--from-file wg_cont.json -o wg_cont_b.cf32)
file(MD5 wg_cont_a.cf32 ca)
file(MD5 wg_cont_b.cf32 cb)
if(NOT ca STREQUAL cb)
    message(FATAL_ERROR "continuous DSSS --from-file replay differs (prbs)")
endif()
run(--type dsss --data-code ${DC} --symbol-rate 2700 --code-only --sps 2
    --fs 6138000 --count 4096 --record wg_cono.json -o wg_cono_a.cf32)
run(--from-file wg_cono.json -o wg_cono_b.cf32)
file(MD5 wg_cono_a.cf32 na)
file(MD5 wg_cono_b.cf32 nb)
if(NOT na STREQUAL nb)
    message(FATAL_ERROR "continuous DSSS --from-file replay differs (none)")
endif()
# Frozen as MD5 goldens, rendered from main (d8f132fc) BEFORE #1619 F6a
# moved code-only DSSS off `--data none` onto its own flag: the seeded
# default and code-only must stay byte-identical across that change of
# spelling (the #853 reviewer's condition on it).
set(WG_CONT_PRBS_GOLDEN "b79a5a35832044c39de575465b6bbb81")
set(WG_CONT_NONE_GOLDEN "090153f8140845efefd31f97bf05bb16")
if(NOT ca STREQUAL WG_CONT_PRBS_GOLDEN)
    message(FATAL_ERROR "continuous DSSS seeded default drifted: got ${ca}, "
                        "want ${WG_CONT_PRBS_GOLDEN}")
endif()
if(NOT na STREQUAL WG_CONT_NONE_GOLDEN)
    message(FATAL_ERROR "continuous DSSS code-only drifted: got ${na}, "
                        "want ${WG_CONT_NONE_GOLDEN}")
endif()
# PRBS and code-only must differ (data modulation is present in one, not both).
if(ca STREQUAL na)
    message(FATAL_ERROR "continuous DSSS: prbs and code-only produced same bytes")
endif()

# 15. Continuous DSSS SigMF sidecar carries wfmgen:symbol_rate (the "dsss" label
#     alone can't distinguish burst from continuous); code-only adds
#     wfmgen:data=none.
run(--type dsss --data-code ${DC} --symbol-rate 2700 --sps 2 --fs 6138000
    --count 512 --file-type sigmf -o wg_cont_cap)
expect_contains(wg_cont_cap.sigmf-meta "\"wfmgen:symbol_rate\":2700")
run(--type dsss --data-code ${DC} --symbol-rate 2700 --code-only --sps 2
    --fs 6138000 --count 512 --file-type sigmf -o wg_cono_cap)
expect_contains(wg_cono_cap.sigmf-meta "\"wfmgen:data\":\"none\"")

# 16. Continuous-DSSS usage rejections (exit 2): incompatible burst-frame flags,
#     --data with a payload, missing --data-code, non-positive --symbol-rate,
#     and --continuous with SigMF (the sidecar can't be written for an unbounded
#     stream).
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --acq-code ${DC})
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --code-only --data 1011)
expect_exit(2 --type dsss --symbol-rate 2700 --sps 2)   # no --data-code
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 0)
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --sps 2 --fs 6138000
    --continuous --file-type sigmf -o wg_bad_cont)

# 17. #1619 F6a, a data source: every refusal a face can decide before the
#     first sample exits 2. `--data` is the Field grammar only, so the old
#     `--data none|prbs` is refused naming its fix; the pair is one
#     exclusion; a finite source sets the run's length, so --count beside it
#     is refused; stdin (`-`) needs --fill and cannot be repeated.
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --data none)
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --data prbs)
expect_exit(2 --type bpsk --data 0xABCD --data-from-file wg_none.bin)
expect_exit(2 --type bpsk --data 0xABCD --data-len 8 --count 64)
expect_exit(2 --type bpsk --data-from-file - --data-len 8)
expect_exit(2 --type bpsk --data-from-file - --data-len 8 --fill 0 --repeat)
expect_exit(2 --type bpsk --data 0xABC --data-len 8)  # 12 bits, no --fill

# 18. #1719, a data source on a dsss BURST: a burst per chunk, so "AB" in
#     8-bit chunks is two bursts of 4 + (8 + 16) * 4 = 100 chips, whether the
#     bits come as a Field or from a file.
file(WRITE wg_ab.bin "AB")
run(--type dsss --acq-code 0x9 --data-code 0xd --data 0x4142 --data-len 8
    --sps 1 -o wg_dsss_field.cf32)
run(--type dsss --acq-code 0x9 --data-code 0xd --data-from-file wg_ab.bin
    --data-len 8 --sps 1 -o wg_dsss_file.cf32)
expect_size(wg_dsss_field.cf32 1600)
file(MD5 wg_dsss_field.cf32 dsf)
file(MD5 wg_dsss_file.cf32 dsg)
if(NOT dsf STREQUAL dsg)
    message(FATAL_ERROR "a dsss burst over a file differs from the Field")
endif()

# 18b. #1718, a carried frame of FIXED bits is a finite source of one frame,
#     sent once: its run is derived (4 bits, 4 samples at sps 1), and a
#     --count beside it is refused, naming --repeats.
file(WRITE wg_fixed.frame.json "{\"fields\":[{\"name\":\"a\",\"spec\":\"1010\"}]}")
run(--type bits --frame wg_fixed.frame.json --sps 1 -o wg_fixed.cf32)
expect_size(wg_fixed.cf32 32)
expect_exit(2 --type bits --frame wg_fixed.frame.json --count 64)

# 19. #1719, CONTINUOUS dsss over a data source: one bit per data symbol, no
#     frame. 16 bits at 6138000 / 2 / 2700 = 1136.67 chips a symbol is
#     ceil(16 * 1136.67) = 18187 chips, 2 samples each -- derived, with no
#     --count -- and a file gives the same as the Field. What only a frame
#     means is refused: --data-len, --fill, and --realtime over stdin.
run(--type dsss --data-code ${DC} --symbol-rate 2700 --sps 2 --fs 6138000
    --data 0x4142 -o wg_cont_field.cf32)
run(--type dsss --data-code ${DC} --symbol-rate 2700 --sps 2 --fs 6138000
    --data-from-file wg_ab.bin -o wg_cont_file.cf32)
expect_size(wg_cont_field.cf32 290992)
file(MD5 wg_cont_field.cf32 cnf)
file(MD5 wg_cont_file.cf32 cng)
if(NOT cnf STREQUAL cng)
    message(FATAL_ERROR "continuous dsss over a file differs from the Field")
endif()
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --sps 2
    --fs 6138000 --data 0x4142 --data-len 8)
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --sps 2
    --fs 6138000 --data 0x4142 --fill 0)
expect_exit(2 --type dsss --data-code ${DC} --symbol-rate 2700 --sps 2
    --fs 6138000 --data-from-file - --realtime)

# 20. doppler#1153: a scene's top-level "fs" is not a key -- fs is per
#     segment -- and it used to be dropped in silence, leaving every segment
#     at fs = 1, so --realtime paced this 7 ms scene for two hours. It is
#     refused by name now, and the same scene with fs where it belongs
#     finishes under --realtime: 4096 + 1024 off + 2048 = 7168 samples,
#     8 bytes each. The TIMEOUT is the regression: a hang fails it.
set(_seg_fs "")
set(_top_fs "\"fs\": 1000000.0, ")
foreach(_where top seg)
    if(_where STREQUAL "seg")
        set(_seg_fs "\"fs\": 1000000.0, ")
        set(_top_fs "")
    endif()
    file(WRITE wg_rt_${_where}.json "{${_top_fs}\"segments\": [
  {${_seg_fs}\"num_samples\": 4096, \"off_samples\": 1024,
   \"sum\": [{\"type\": \"bpsk\", \"sps\": 4, \"snr\": 10.0}]},
  {${_seg_fs}\"num_samples\": 2048,
   \"sum\": [{\"type\": \"tone\", \"freq\": 100000.0}]}]}
")
endforeach()
execute_process(COMMAND ${EXE} --from-file wg_rt_top.json --realtime
                -o wg_rt_top.cf32
                RESULT_VARIABLE rc ERROR_VARIABLE err TIMEOUT 20)
if(NOT rc EQUAL 2 OR NOT err MATCHES "set segments\\[\\]\\.fs")
    message(FATAL_ERROR "a top-level fs: exit ${rc}, '${err}' -- expected "
                        "exit 2 naming segments[].fs")
endif()
execute_process(COMMAND ${EXE} --from-file wg_rt_seg.json --realtime
                -o wg_rt_seg.cf32
                RESULT_VARIABLE rc TIMEOUT 20)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "a finite scene under --realtime: exit '${rc}' "
                        "(a hang reads as a timeout) -- expected 0")
endif()
expect_size(wg_rt_seg.cf32 57344)

# 21. doppler#1733: a scene whose segments differ in fs is legal, and each
#     output either says so honestly or refuses. BLUE states one rate (one
#     xdelta), so attached and detached both refuse, naming the two rates;
#     SigMF leaves core:sample_rate out; raw states none and is written.
file(WRITE wg_mixed.json "{\"segments\": [
  {\"fs\": 6000000.0, \"num_samples\": 600,
   \"sum\": [{\"type\": \"tone\", \"freq\": 1000.0}]},
  {\"fs\": 2000000.0, \"num_samples\": 400,
   \"sum\": [{\"type\": \"tone\", \"freq\": 1000.0}]}]}
")
foreach(_detached "" "--detached")
    execute_process(COMMAND ${EXE} --from-file wg_mixed.json
                    --file-type blue ${_detached} -o wg_mixed_blue
                    RESULT_VARIABLE rc ERROR_VARIABLE err)
    if(NOT rc EQUAL 2 OR NOT err MATCHES "different fs \\(6e\\+06, 2e\\+06\\)")
        message(FATAL_ERROR "a mixed-fs scene as BLUE ${_detached}: exit "
                            "${rc}, '${err}' -- expected 2, naming both")
    endif()
endforeach()
run(--from-file wg_mixed.json --file-type sigmf -o wg_mixed)
file(READ wg_mixed.sigmf-meta _meta)
string(FIND "${_meta}" "core:sample_rate" _at)
if(NOT _at EQUAL -1)
    message(FATAL_ERROR "a mixed-fs scene's SigMF states a sample rate")
endif()
run(--from-file wg_mixed.json -o wg_mixed.cf32)
expect_size(wg_mixed.cf32 8000)

sweep_scratch()
message(STATUS "wfmgen_cli: OK")
