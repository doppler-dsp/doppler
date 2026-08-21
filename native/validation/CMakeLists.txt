# Hand-owned Monte-Carlo validation harnesses (NOT jm-managed): each proves a
# component meets its theoretical bound. They link the static core library
# (pn + awgn + the component cores) and register a fast `--check` CTest.

add_executable(validate_ber_despreader
               ${CMAKE_CURRENT_SOURCE_DIR}/ber_despreader.c)
target_include_directories(validate_ber_despreader
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_ber_despreader PRIVATE doppler_lib_static m)
add_test(NAME validate_ber_despreader
         COMMAND validate_ber_despreader --check)
# BER Monte-Carlo: allow FP-reduction vectorization (the despread is a plain
# dot product; sign decision is insensitive to reassociation).
target_compile_options(validate_ber_despreader PRIVATE
                       -O3 -funroll-loops -ffast-math)

add_executable(validate_costas_jitter
               ${CMAKE_CURRENT_SOURCE_DIR}/costas_jitter.c)
target_include_directories(validate_costas_jitter
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_costas_jitter PRIVATE doppler_lib_static m)
target_compile_options(validate_costas_jitter PRIVATE -O3 -funroll-loops)
add_test(NAME validate_costas_jitter
         COMMAND validate_costas_jitter --check)

add_executable(validate_dll_jitter
               ${CMAKE_CURRENT_SOURCE_DIR}/dll_jitter.c)
target_include_directories(validate_dll_jitter
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_dll_jitter PRIVATE doppler_lib_static m)
# -ffp-contract=off: the closed-loop integrator (`integ += ki*x` in
# loop_filter_step, `integ*inv_tsamps + kp*e*inv_tsamps_sf` in dll_update)
# fuses into a single fmadd under the default -ffp-contract=fast on hosts with
# baseline FMA (arm64/AArch64 NEON), but stays separately-rounded on x86-64
# without -mfma. That per-epoch LSB difference compounds over the 5000-8000
# epoch Monte-Carlo and lands the loop on a different jitter, tripping the
# knife-edge nxn>=1.05 gate on macOS/arm64 while passing on x86. Disabling
# contraction makes both platforms round every multiply-add separately (the
# gates are tuned once against that), so the harness is host-deterministic.
target_compile_options(validate_dll_jitter PRIVATE -O3 -funroll-loops
                                                   -ffp-contract=off)
add_test(NAME validate_dll_jitter
         COMMAND validate_dll_jitter --check)

add_executable(validate_carrier_mpsk_scurve
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_mpsk_scurve.c)
target_include_directories(validate_carrier_mpsk_scurve
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_mpsk_scurve PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_mpsk_scurve PRIVATE -O3)
add_test(NAME validate_carrier_mpsk_scurve
         COMMAND validate_carrier_mpsk_scurve --check)

add_executable(validate_carrier_mpsk_jitter
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_mpsk_jitter.c)
target_include_directories(validate_carrier_mpsk_jitter
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_carrier_mpsk_jitter PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_mpsk_jitter PRIVATE -O3 -funroll-loops)
add_test(NAME validate_carrier_mpsk_jitter
         COMMAND validate_carrier_mpsk_jitter --check)

add_executable(validate_carrier_nda_scurve
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_scurve.c)
target_include_directories(validate_carrier_nda_scurve
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_nda_scurve PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_scurve PRIVATE -O3)
add_test(NAME validate_carrier_nda_scurve
         COMMAND validate_carrier_nda_scurve --check)

add_executable(validate_carrier_nda_pullin
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_pullin.c)
target_include_directories(validate_carrier_nda_pullin
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_carrier_nda_pullin PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_pullin PRIVATE -O3 -funroll-loops)
add_test(NAME validate_carrier_nda_pullin
         COMMAND validate_carrier_nda_pullin --check)

add_executable(validate_carrier_nda_step_response
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_step_response.c)
target_include_directories(validate_carrier_nda_step_response
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_carrier_nda_step_response
                      PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_step_response PRIVATE -O3)
add_test(NAME validate_carrier_nda_step_response
         COMMAND validate_carrier_nda_step_response --check)

# The two M-PSK BER validators share one stimulus + measurement harness
# (mpsk_ber_common.h) so the complex and real paths stay comparable, and both
# score through native/tests/dp_ber_test.h -- hence the extra include dir.
# dp_ber_test.h needs detection_core + snr_core, which doppler_lib_static
# already carries.
add_executable(validate_mpsk_receiver_ber
               ${CMAKE_CURRENT_SOURCE_DIR}/mpsk_receiver_ber.c)
target_include_directories(validate_mpsk_receiver_ber
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_mpsk_receiver_ber PRIVATE doppler_lib_static m)
target_compile_options(validate_mpsk_receiver_ber PRIVATE -O3 -funroll-loops)
add_test(NAME validate_mpsk_receiver_ber
         COMMAND validate_mpsk_receiver_ber --check)

add_executable(validate_mpsk_receiver_real_ber
               ${CMAKE_CURRENT_SOURCE_DIR}/mpsk_receiver_real_ber.c)
target_include_directories(validate_mpsk_receiver_real_ber
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_mpsk_receiver_real_ber PRIVATE doppler_lib_static m)
target_compile_options(validate_mpsk_receiver_real_ber PRIVATE -O3 -funroll-loops)
add_test(NAME validate_mpsk_receiver_real_ber
         COMMAND validate_mpsk_receiver_real_ber --check)

add_executable(validate_symsync_lock
               ${CMAKE_CURRENT_SOURCE_DIR}/symsync_lock.c)
target_include_directories(validate_symsync_lock
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_symsync_lock PRIVATE doppler_lib_static m)
target_compile_options(validate_symsync_lock PRIVATE -O3 -funroll-loops)
add_test(NAME validate_symsync_lock
         COMMAND validate_symsync_lock --check)

add_executable(validate_carrier_nda_lock
               ${CMAKE_CURRENT_SOURCE_DIR}/carrier_nda_lock.c)
target_include_directories(validate_carrier_nda_lock
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_carrier_nda_lock PRIVATE doppler_lib_static m)
target_compile_options(validate_carrier_nda_lock PRIVATE -O3 -funroll-loops)
add_test(NAME validate_carrier_nda_lock
         COMMAND validate_carrier_nda_lock --check)

# Each TED's own S-curve against symsync_ted_slope(), with no cascade in the
# path: the composite pulse is evaluated in closed form, so a discrepancy is
# the detector's or the model's and cannot be the bank's. -ffp-contract=off
# for the same host-determinism reason as dll_jitter: the measurement is a
# paired central difference of two large sums, and a fused multiply-add
# changes its last digits between x86-64 and arm64.
add_executable(validate_ratesync_scurve
               ${CMAKE_CURRENT_SOURCE_DIR}/ratesync_scurve.c)
# native/tests too: phase 3 drives dp_tx_make(), the shared harness
# stimulus the unit tests use, rather than re-shaping a pulse here.
target_include_directories(validate_ratesync_scurve
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_ratesync_scurve PRIVATE doppler_lib_static m)
target_compile_options(validate_ratesync_scurve PRIVATE -O3 -funroll-loops
                                                        -ffp-contract=off)
add_test(NAME validate_ratesync_scurve
         COMMAND validate_ratesync_scurve --check)

add_executable(validate_lockdet_verify
               ${CMAKE_CURRENT_SOURCE_DIR}/lockdet_verify.c)
target_include_directories(validate_lockdet_verify
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_lockdet_verify PRIVATE doppler_lib_static m)
target_compile_options(validate_lockdet_verify PRIVATE -O3 -funroll-loops)
add_test(NAME validate_lockdet_verify
         COMMAND validate_lockdet_verify --check)

add_executable(validate_mpsk_diff_penalty
               ${CMAKE_CURRENT_SOURCE_DIR}/mpsk_diff_penalty.c)
target_include_directories(validate_mpsk_diff_penalty
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_mpsk_diff_penalty PRIVATE doppler_lib_static m)
target_compile_options(validate_mpsk_diff_penalty PRIVATE -O3 -funroll-loops)
add_test(NAME validate_mpsk_diff_penalty
         COMMAND validate_mpsk_diff_penalty --check)

add_executable(validate_loop_filter_noise_bw
               ${CMAKE_CURRENT_SOURCE_DIR}/loop_filter_noise_bw.c)
target_include_directories(validate_loop_filter_noise_bw
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_loop_filter_noise_bw PRIVATE doppler_lib_static m)
# -ffp-contract=off: the measurement IS an accumulated integrator, so a fused
# multiply-add inside loop_filter_step moves the last bits of a sum taken over
# hundreds of thousands of updates. The 1% gate is nowhere near that
# sensitive, but the printed table is compared across machines.
target_compile_options(validate_loop_filter_noise_bw PRIVATE
                       -O3 -funroll-loops -ffp-contract=off)
add_test(NAME validate_loop_filter_noise_bw
         COMMAND validate_loop_filter_noise_bw --check)

# The end-to-end receiver run: a NAMED frame (native/tests/dp_frame_test.h)
# through wfm_synth, through MpskReceiver, scored into ber + snr + frame_meter
# so BER, EVM, M2M4 and FER come out of one record. It is the first FER
# measured on a receiver rather than on synthetic outcomes, and the first place
# dp_ber_marker_t's `period` is fed by an actual periodic sync word -- until
# wfm_frame there was nothing in the tree that could emit one outside DSSS.
# The four NDA carrier taps, ranked by how much of a KNOWN offset each one
# removes. Nothing exercised any tap but STROBE before this harness, and the
# obvious zero-offset experiment ranks them backwards (a loop that never
# steers reports a perfect error) -- see the file docstring.
add_executable(validate_rx_battery
               ${CMAKE_CURRENT_SOURCE_DIR}/rx_battery.c)
target_include_directories(validate_rx_battery
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_rx_battery PRIVATE doppler_lib_static m)
target_compile_options(validate_rx_battery PRIVATE -O3 -funroll-loops)
add_test(NAME validate_rx_battery
         COMMAND validate_rx_battery --check)

# The receiver under a COUPLED Doppler ramp across a data onset -- the
# continuous flavor's own scenario (NRZ BPSK, modulation off then dense), which
# neither rx_battery (RRC, dense throughout) nor the retired tap sweep (NRZ, noiseless, no
# dynamics) covers. Writes a dp_tlm capture per tap with --out, so the report
# plots the dynamics instead of restating a table.
add_executable(validate_rx_dynamics
               ${CMAKE_CURRENT_SOURCE_DIR}/rx_dynamics.c)
target_include_directories(validate_rx_dynamics
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
# Threads::Threads explicitly: dp_tlm_capture's writer thread is the only
# pthread user any validator pulls in, and doppler_lib_static aggregates it as
# OBJECT sources, which carry no usage requirement to propagate. On glibc >=
# 2.34 pthread_create lives in libc proper so this links by accident; the
# glibc 2.28 gate is where the missing -lpthread actually shows up.
target_link_libraries(validate_rx_dynamics PRIVATE doppler_lib_static
                                                   Threads::Threads m)
target_compile_options(validate_rx_dynamics PRIVATE -O3 -funroll-loops)
add_test(NAME validate_rx_dynamics
         COMMAND validate_rx_dynamics --check)


# conv_certify has no `--check` and registers no CTest, and that is the
# design rather than an omission: it MEASURES and the limits are asserted in
# src/doppler/tests/validation/conv/, by the same Report machinery every other
# certified object uses. Two places deciding what an acceptable BER is would
# be two envelopes. `make validate-c` still runs it (it discovers harnesses
# from git, so this needs no registration), and `make build` builds it, which
# is what the Python limits test depends on.
add_executable(validate_conv_certify
               ${CMAKE_CURRENT_SOURCE_DIR}/conv_certify.c)
target_include_directories(validate_conv_certify
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc)
target_link_libraries(validate_conv_certify PRIVATE doppler_lib_static m)
target_compile_options(validate_conv_certify PRIVATE -O3 -funroll-loops)

# rs_certify is conv_certify's shape, for the same reason: it MEASURES, and
# the limits are asserted in src/doppler/tests/validation/rs/. No `--check`,
# no CTest. It reaches native/tests for dp_rng_test.h — the error PATTERNS the
# sphere sweep places are the experiment's independent variable, not a signal,
# and it places them with the generator test_rs_core.c places them with.
add_executable(validate_rs_certify
               ${CMAKE_CURRENT_SOURCE_DIR}/rs_certify.c)
target_include_directories(validate_rs_certify
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_rs_certify PRIVATE doppler_lib_static m)
target_compile_options(validate_rs_certify PRIVATE -O3 -funroll-loops)

# ccsds_tm_certify is the third of the C-only certifications, and measures the
# three things this component adds ON TOP of the code it configures: the
# interleaver's burst tolerance, the sync marker as a detector, and the
# spectrum B-6 demoted the legacy randomiser over. No `--check`, no CTest --
# the limits are asserted in src/doppler/tests/validation/ccsds_tm/.
add_executable(validate_ccsds_tm_certify
               ${CMAKE_CURRENT_SOURCE_DIR}/ccsds_tm_certify.c)
target_include_directories(validate_ccsds_tm_certify
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_ccsds_tm_certify PRIVATE doppler_lib_static m)
target_compile_options(validate_ccsds_tm_certify PRIVATE -O3 -funroll-loops)

add_executable(validate_rx_coding_gain
               ${CMAKE_CURRENT_SOURCE_DIR}/rx_coding_gain.c)
target_include_directories(validate_rx_coding_gain
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_rx_coding_gain PRIVATE doppler_lib_static m)
target_compile_options(validate_rx_coding_gain PRIVATE -O3 -funroll-loops)
add_test(NAME validate_rx_coding_gain
         COMMAND validate_rx_coding_gain --check)

add_executable(validate_rx_frame_fer
               ${CMAKE_CURRENT_SOURCE_DIR}/rx_frame_fer.c)
target_include_directories(validate_rx_frame_fer
                           PRIVATE ${CMAKE_SOURCE_DIR}/native/inc
                                   ${CMAKE_SOURCE_DIR}/native/tests)
target_link_libraries(validate_rx_frame_fer PRIVATE doppler_lib_static m)
target_compile_options(validate_rx_frame_fer PRIVATE -O3 -funroll-loops)
add_test(NAME validate_rx_frame_fer
         COMMAND validate_rx_frame_fer --check)
