<!-- domain: acquire; summary: step 3 of the correlator consolidation (#2752): the seven correlation objects become one C object, correlation/correlator/, its layers and constructors, the duplications it removes, the one deliberate C break, the unknowns, and the sub-steps with their oracles and speed gates -->

# One correlator: the step-3 design

Step 3 of the consolidation ([#2752](https://github.com/doppler-dsp/doppler/issues/2752),
[API taxonomy §6.4](api-taxonomy.md#64-consolidation-one-primitive-correlator)).
Six C directories hold seven public classes that compute one thing:
correlate input against a reference, then perhaps threshold it. Step 3 makes
them one C object, `native/{inc/doppler,src}/correlator/`, and retires
`corr/`, `corr2d/`, `detector/`, `detector2d/` and `acq/`. `burst_acq/` goes
in step 4 with the Python classes (taxonomy §6.4, step 1 folded).

Step 2 already put the delay-Doppler search in one place: acq's two paths run
on the map in `corr2d_core.c` ([acq-surface §18](acq-surface.md#18-one-delay-doppler-map)).
Step 3 does the same for everything around it: one framing, one detection
layer, one peak picker and one state blob.

**The owner's two rules govern every sub-step** (2026-11-02):

1. **No private copy survives.** When a sub-step lands, the code it
    consolidates exists once.
1. **Speed only goes up.** Every benchmark row the sub-step touches is equal
    or faster on the bench box, judged by `make bench-ab` against its budget
    (§7.1).

This is a redesign of certified objects, so it follows
[Adding an algorithm](../dev/contributing/adding-algorithms.md) from phase 1,
and the reports are re-certified on the one object.

## 1. Who calls it, and with what

| caller                                               | object today                     | shape                                       | does with the answer                                       |
| ---------------------------------------------------- | -------------------------------- | ------------------------------------------- | ---------------------------------------------------------- |
| `BurstCapture`                                       | `Acquisition` (burst door)       | stream of samples, code row × Doppler       | resolves the period, refines, retains, emits a burst       |
| `DsssReceiver`, `AsyncDsssReceiver`, `AsyncDsssPool` | `Acquisition` (continuous door)  | stream, code row × Doppler                  | hands the hit to the tracking loops                        |
| `CarrierAcquisition`                                 | `CorrDetector`                   | stream of averaged PSD frames, 1-D template | sub-bin carrier estimate                                   |
| Python users                                         | `Corr`, `Corr2D`, `DelayDoppler` | whole frames                                | the correlation map, batch reductions, the hypothesis bank |
| Python users                                         | `CorrDetector`, `CorrDetector2D` | stream                                      | detections over a k × noise gate                           |

Nothing else in `native/` correlates by FFT: `ppe_core.c` is a periodogram.

**`carrier_acq` is coupled to `CorrDetector` three ways**, and sub-step 3.3
changes all three:

- it nests the detector's state blob in its own (`carrier_acq_core.c:368`,
    `:388`, `:423`), so `CAQR`'s version bumps when the detector's blob
    becomes the correlator's;
- it reads the detector's correlation output directly,
    `s->det->out_buf` (`:207-209`), for the parabolic refine;
- it calls `dp_detector_set_threshold` on every push (`:298`).

**Everything else that touches the retired objects**, each in the oracle of
the sub-step that changes it:

| kind           | files                                                                                                                                                                      |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| examples       | `native/examples/corr_demo.c`; `src/doppler/examples/corr_demo.py`, which draws `docs/gallery/corr.md`; `src/doppler/examples/dsss_burst_demo.py:210` (`Corr` and its `n`) |
| docs           | `docs/api/python-spectral.md`, `docs/api/python-detection.md`, `docs/gallery/corr.md`                                                                                      |
| Python suites  | `src/doppler/spectral/tests/test_corr.py`, `test_detector.py`, `test_exact_length.py`; the `src/doppler/tests/test_state_serialization.py` matrix                          |
| C symbol tests | `native/tests/correlator/test_correlator_1d_symbols.c` (Corr's, moved in 3.2), `native/tests/detector/test_detector_symbols.c`                                             |
| bookkeeping    | `scripts/.mutator-state-exempt`, `native/inc/.example-arity-ratchet`, `native/.c-header-code-ignore`, `scripts/.doc-symbol-exempt`                                         |

## 2. Today, measured

Origin/main at `ba70f133c`, 2026-11-05.

| object                    |              core lines (`.h` + `.c`) | engine                                                        | C tests (`DP_CHECK`) | certified                      |
| ------------------------- | ------------------------------------: | ------------------------------------------------------------- | -------------------: | ------------------------------ |
| `Corr`                    |                             241 + 200 | own 1-D FFT plans                                             |                   58 | no                             |
| `CorrDetector`            |                             324 + 249 | `Corr`, ring, framer, own argmax                              |                   78 | no                             |
| `Corr2D` / `DelayDoppler` |  1,066 + 2,517 (+ 331 `corr2d_map.h`) | 2-D FFT, the single-row path, the delay-Doppler map, the bank |                  260 | yes, 30/30                     |
| `CorrDetector2D`          |                             339 + 237 | `Corr2D`, ring, framer, `det_peak_list`                       |                  140 | yes, 15/15                     |
| `Acquisition`             | 2,324 + 3,510 (+ 119 `acq_private.h`) | the map, its own framer, `det_private.h`'s CFAR and peaks     |                  597 | yes, 22/22 (two findings open) |
| `BurstAcquisition`        |                              329 + 92 | a forwarder onto `Acquisition`                                |                   12 | yes, 16/16                     |

The shared CFAR is `native/inc/doppler/detector/det_private.h`: header-only
statics for the ring (`det_ring_create`), the noise estimate
(`det_noise_chunk`, `det_noise_estimate`), the peaks (`det_peak_list`,
`det_peak_scan`, `det_peak_zone`) and the framed push (`det_framed_push`).
`CorrDetector`, `CorrDetector2D` and acq include it.

### 2.1 The 1-D pair is the 2-D pair at one row

The arithmetic is the same, in the same order. `dp_corr_execute`
(`corr_core.c:154-194`) and `Corr2D`'s single-row path (`corr2d_row` and
`corr2d_row_inverse`, `corr2d_core.c:428-459`) take:

- the same plans (`dp_fft_create` ignores `nthreads`);
- the same multiply-accumulate against the conjugated reference spectrum;
- the same `dp_fft_pad_spectrum`;
- the same scale, `1/n` after the inverse (`1/n == 1/nx`).

`det_peak_scan` with no mask is `CorrDetector`'s strict-`>` argmax.

A throwaway Python prototype agreed:

- `Corr` against `Corr2D` with `ny = 1`: 32 maps byte-identical, over n = 31,
    64, 100 and 1023, dwells 1 and 3, and lag pads 1 and 2.
- `CorrDetector` against `CorrDetector2D` with `ny = 1`: 110 hits over 48
    streams identical. The streams covered the four noise modes, dwells 1
    and 2 and n = 31, 64 and 127, pushed in odd chunks.

The prototype is evidence for this page only; nothing reruns it. Sub-step
3.2 opens by committing it as a C test (§7).

## 3. What is duplicated, which rule 1 removes

|        | the copies                                                                                                                                                                                                               | becomes                                                                               |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------- |
| **D1** | `Corr`'s 1-D correlation (`corr_core.c:154-194`) and `Corr2D`'s single-row path (`corr2d_core.c:428-459`)                                                                                                                | the single-row path, at `ny = 1` (§2.1)                                               |
| **D2** | `CorrDetector`'s argmax loop (`detector_core.c:49-52`) and `det_peak_list`                                                                                                                                               | `det_peak_list` with one peak; NaN by U2's rule                                       |
| **D3** | `Corr2D`'s own peak passes: `corr2d_fold_peaks` (`corr2d_core.c:1072-1106`) and the bank's 3 × 3 local maximum (`corr2d_core.c:1771-1808`), beside `det_peak_scan` and `det_peak_zone`                                   | one peak primitive in the correlator's private header, if the measurements agree (U3) |
| **D4** | the dwell counter, `++count == dwell` (`corr_core.c:160`, `corr2d_core.c:620`, `:632`)                                                                                                                                   | the single-row path's, with D1                                                        |
| **D5** | acq's setup-time n-point transform pairs outside the map: `acq_shape_of_template`, `acq_off_peak`, `acq_doppler_sidelobe` and `dp_acq_sidelobe_ratio` (`acq_core.c:1912-1913`, `:1975-1976`, `:2041-2042`, `:3009-3011`) | decided by U4: they run in `double _Complex`, and the single-row path in cf32         |

**Not duplicates:**

- the three rings and framers, which all go through `det_ring_create` and
    `dp_f32_framer`, one primitive already;
- `det_peak_list`'s two appearances in `det_private.h` (`:191` and `:317`),
    which are a forward declaration and its definition.

## 4. The one object

One state type, `dp_correlator_state_t`, in four layers. Each layer is used
by the constructors that need it, and none exists twice.

| layer         | holds                                                                                                                                            | from today                                           |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------- |
| **map**       | the reference spectrum and the transforms: the general 2-D shape, the single-row shape (1-D at `ny = 1`) and the delay-Doppler map with its bank | `corr2d_core.c` and `corr2d_map.h`                   |
| **dwell**     | the coherent accumulator and its count; the non-coherent sum                                                                                     | `Corr2D`'s, and acq's `nc_surface`                   |
| **stream**    | the ring and framer, block-coherent epochs held raw                                                                                              | `det_private.h`'s ring and framed push, acq's framer |
| **detection** | magnitude, noise estimate, peak list, the gate (k × noise, or from Pfa and Pd), twins, the decision                                              | `det_private.h`, acq's CFAR                          |

acq's search policy sits on top of the four layers and stays in the object:
the two front doors' sizing (`acq_auto_config_*`), the rate hypotheses, the
carrier and its rebuild, the sidelobe floor, the hand-off, the telemetry and
the surface sink.

**The constructors are flavors, the methods shared.** Today's constructors
become the object's, renamed and otherwise unchanged in their arguments:

| today                                             | becomes                                                         | layers                           |
| ------------------------------------------------- | --------------------------------------------------------------- | -------------------------------- |
| `dp_corr_create`                                  | `dp_correlator_create_1d`                                       | map (single row) and dwell       |
| `dp_corr2d_create`                                | `dp_correlator_create_2d`                                       | map and dwell                    |
| `dp_corr2d_create_delay_doppler`                  | `dp_correlator_create_delay_doppler`                            | map (delay-Doppler) and dwell    |
| `dp_detector_create`                              | `dp_correlator_create_detector_1d`                              | all four, single row             |
| `dp_detector2d_create`                            | `dp_correlator_create_detector_2d`                              | all four                         |
| `dp_acq_create_continuous`, `dp_acq_create_burst` | `dp_correlator_create_continuous`, `dp_correlator_create_burst` | all four, with the search policy |

`execute` and its batch family serve the map constructors, and `push`
serves the detection ones. A method that cannot serve a flavor refuses it
(`SIZE_MAX`, as `execute` refuses a short frame today), never falls back.

**One hit record** (owner, 2026-11-05). `det_result_t` (lag),
`det_result2d_t` (row, col) and `acq_result_t` (row, col and the search's
fields) are three records for one event. The correlator has one:
`acq_result_t`'s fields under the correlator's name, with `row = 0` at one
row.

**One state blob.** One magic, `CORL`, and one version. The flavor is the
blob's first field and a reject key, like the grid today. The payload is
each layer's own fields, packed by that layer once.

## 5. The one deliberate C break

The owner ruled it on 2026-11-05: the C renames and the retired state magics
are one break, listed under **Breaking** in the release that ships them.

- **Every `dp_corr_*`, `dp_corr2d_*`, `dp_detector_*`, `dp_detector2d_*` and
    `dp_acq_*` symbol becomes `dp_correlator_*`,** with no deprecation shims
    (the doppler#2753 policy: before 1.0, a break happens at once).
- **The retired magics are refused, never misread.** `CORR`, `CR2D`, `DDMP`,
    `DET1`, `DET2` and `ACQR` have shipped in a release, and
    `docs/design/state-serialization.md` holds that a released version names
    its layout for good (`make state-versions-check` reads every release
    tag). So they are not reused. `CORL` is a new magic, and a blob saved
    by any release before the break fails `dp_state_validate` cleanly.
- **The composing blobs that nest one bump their own versions:**
    `carrier_acq`'s `CAQR` (in 3.3), and acq's consumers in 3.4,
    `BurstCapture`, the receivers and the pool, each named in that
    sub-step's PR.

**The Python classes keep their surfaces until step 4**, so a Python user
meets one break, at step 4. One jm object, `correlator`, carries today's
classes as `[[correlator.views]]`. Each view has its own `class_name`,
`create_fn` and `init_params`, and `exclude_methods` and
`exclude_properties` trim what a flavor does not serve, as `ddc` does for
`MatchedDDC`.

A view can also override a parent method under the same Python name, with
a `fn` of its own: `MpskReceiverR`'s `[[mpsk_receiver.views.methods]]`
`steps` and `bits` are the precedent. The 1-D detector view overrides `push`
with a C function that returns today's `(lag, peak, noise, stat)`.

**The rule for each view's surface:** a view excludes every parent member
its class does not have today, and adds none, so each class keeps exactly
today's members, names and signatures. The parent carries the union of
today's five surfaces:

- the map methods `execute`, `execute_all`, `execute_coherent`,
    `execute_power_sum`, `execute_power_max`, `execute_peaks`,
    `bank_power_max` and `bank_detect`;
- the detection method `push`;
- the properties `n`, `ny`, `nx`, `ny_out`, `nx_out`, `n_out`, `col_out`,
    `dwell`, `count`, `ring_cap`, `noise_lo`, `noise_hi`, `threshold` and
    `last_corr`.

`DelayDoppler` stays a view of `Corr2D`'s flavor, with today's
`exclude_properties = ["nx_out", "col_out"]`. The differences between a
1-D class and its 2-D twin are these, and only these:

| view           | differs from its 2-D twin                                                                                                                 | carried by                                         |
| -------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- |
| `Corr`         | it has `n`, which `Corr2D` lacks, and lacks `ny`, `nx`, `ny_out`, `nx_out`, `col_out` and every `execute_*` and `bank_*` beyond `execute` | the rule: `n` kept, the rest excluded              |
| `CorrDetector` | `push` returns `(lag, peak, noise, stat)`, not `(row, col, peak, noise, stat)`; it lacks `ny` and `nx`                                    | the view's own `push` `fn`; `ny` and `nx` excluded |

Both detectors already read `dwell` and `count` the same way, as `expr`s
over their composed correlator (`objects/detector.toml` and
`objects/detector2d.toml`), so those become fields of the one state for
both. `set_ref`, `void` in 1-D and `int` in 2-D, is C-only: no class binds
it, so the C rename (above) covers it and no view does.

Each sub-step's PR lists its views' `exclude_*` as the rule gives them, and
`make drift-check` and `make test-stubs` hold the stubs to today's.

## 6. Goals, and what is not known yet

**Goals:**

- rule 1 and rule 2;
- every retired object's C tests passing against its replacing flavor, and
    every Python suite, example and doc fence that touches it;
- the certified reports (Corr2D, CorrDetector2D, Acquisition,
    BurstAcquisition) re-certified on the one object with no limit lost.

**Unknown, and measured in the sub-step that needs it.** Each names its row
or its input:

- **U1. The short 1-D rows through the single-row path.** §2.1 shows the
    bits agree, not that the speed does. `Corr2D` adds per-worker plans and
    pool dispatch, which a short frame may not amortize. Today's rows run at
    n = 1024 and 16384 (`corr::execute[accumulate]`, `execute[dump]`) and
    chunks of 64 and up (`detector::push[chunk=…]`), where dispatch is
    invisible. So the small-n rows of §7.1 land on main first, and U1 is
    their `bench-ab` verdict: `corr` at n = 31, 63 and 127, and `detector` on
    frames of 31, 63 and 127.
- **U2. Ties and NaN in the one peak picker.** On equal cells,
    `det_peak_list` with one peak keeps `CorrDetector`'s lowest index (§2.1).
    On NaN they differ. Through `det_peak_list` with `gate = -1`, a NaN map
    gives an empty list, so the peak is 0 (`detector2d_core.c:30`), while
    `CorrDetector` reports NaN at lag 0.
    - **Decided here: NaN is never a peak**, the family's rule already
        (`acq_rate_better`'s "a NaN never displaces"). A NaN statistic fails
        every gate, so no hit changes; only the 1-D detector's stored peak
        does.
    - Pinned in C in 3.3: an all-NaN frame and a frame with one NaN cell
        beside a finite peak, on both detector flavors.
- **U3. The bank's peaks on `det_peak_*`.** Whether `corr2d_fold_peaks` and
    the bank's local maximum are `det_peak_scan` / `det_peak_zone` under
    other names, or a different rule (a 3 × 3 window, where acq zones by
    span). Input: the bank's own C test scenes (`test_corr2d_core.c`, the
    `bank_*` cases). If the rule differs, the difference is stated and one
    rule is chosen with the owner.
- **U4. acq's setup transforms: precision first, then speed.** The four
    pairs in D5 run in `double _Complex`, and the single-row path in cf32,
    so moving them is a precision change, not only a refactor.
    - **Measured on:** every acq validator's `--check` verdict, and the setup's
        own outputs (template shape, off-peak floor, Doppler sidelobe,
        sidelobe ratio) on the bench rows' templates.
    - **The decision rule:** if cf32 moves no verdict and changes no output by
        more than its stated use tolerates, the pairs that are correlations
        move onto the single-row path. Otherwise they stay in double, and the
        page records them as a different precision, which rule 1 does not
        forbid, with the owner's ruling.

## 7. The sub-steps

Each is its own PR, tracked by an issue of its own under #2752.

1. **3.1, this page.**

1. **3.1b, the small-n rows, on main first.** On today's code, so their
    bench-box numbers are U1's baseline:

    - `bench_corr_core.c` gains `corr::execute[accumulate][n=31]`,
        `[n=63]` and `[n=127]`, and `corr::execute[dump][n=31]`, `[n=63]`
        and `[n=127]`;
    - `bench_detector_core.c` gains `detector::push[frame=31]`,
        `[frame=63]` and `[frame=127]`, each pushing one frame a call.

    **Each row checks what it timed**, #2832's pattern. Its input carries
    the reference at a planted lag; after the timing, the row asserts the
    peak is at that lag (the map's argmax, or the detector's hit), and
    `exit 1` otherwise. The PR proves each check red by sabotage: plant the
    reference one lag off.

1. **3.2, the map and the 1-D detector's engine.** Nothing is deleted until
    the evidence is committed and green:

    1. a C test: `dp_correlator_create_1d` against `create_2d` at `ny = 1`,
        maps and hits, byte for byte, proven red by sabotage. It runs first
        against today's `dp_corr_*` and `dp_corr2d_*` (the equivalence
        test), then against the flavors;
    1. `correlator/` is created from `corr2d/`, and `Corr` becomes the
        single-row flavor (D1, D4);
    1. **Both detectors are repointed in the same PR:** `detector_core.c:99`
        calls `dp_corr_create` and `detector2d_core.c:83` calls
        `dp_corr2d_create`, so they compose `create_1d` and `create_2d`
        before `corr/` and `corr2d/` are deleted. Their blobs, `DET1` and
        `DET2`, then nest a `CORL` child and bump, and so does `CAQR`, which
        nests `DET1`. 3.3's further change to all three rides the same
        unreleased versions, so no release is cut between 3.2 and the end
        of 3.4; if one is, each bumps again.

    The oracle:

    - `test_corr_core.c`, `test_corr2d_core.c`, `test_corr_symbols.c`;
    - `test_corr.py`, `test_exact_length.py`, the state matrix;
    - `corr_demo.c`, `corr_demo.py`, `dsss_burst_demo.py`;
    - `python-spectral.md`'s fences.

1. **3.3, detection.** `CorrDetector` and `CorrDetector2D` become the
    detector flavors, `det_private.h` the correlator's private header, and
    `detector/` and `detector2d/` are deleted (D2, D3). `carrier_acq`
    composes the 1-D detector flavor: its nested blob, its read of the
    correlation output and its per-push threshold all move, and `CAQR`
    bumps.

    The oracle:

    - **an equivalence run before the switch:** a C test pushes the same
        streams through `dp_detector_*` and through `dp_detector2d_*` at
        `ny = 1`, today's two objects. It asserts every hit identical,
        `(lag, peak, noise, stat)` against `(0, col, peak, noise, stat)`,
        over the four noise modes, dwells 1 and 2, three lengths and odd
        chunks (§2.1's prototype, committed), proven red by sabotage. It then
        runs against the two detector flavors;
    - `test_detector_core.c`'s streaming oracle, ported verbatim: chunk
        invariance, `consumed` and re-offer, `reset`, and `set_state`
        mid-stream;
    - `test_detector2d_core.c`, `test_detector_symbols.c`;
    - `carrier_acq`'s C and Python tests;
    - `test_detector.py`, the state matrix;
    - `python-detection.md`'s and `python-spectral.md`'s fences;
    - U2's NaN pins.

1. **3.4, the search.** acq's engine moves in as the two search flavors, and
    `acq/` is deleted. D5 follows U4's decision. `BurstCapture`, the
    receivers and the pool call `dp_correlator_*`, and `burst_acq` forwards
    to the new names until step 4.

    The oracle:

    - an equivalence test against today's acq before the switch, as step 2's
        was;
    - `test_acq_core.c`, `test_acq_regrid_failure.c`;
    - every acq validator's `--check` verdict unchanged;
    - the composing objects' C and Python tests.

1. **3.5, re-certification.** One report for the one object, its limits
    the union of today's four reports. acq's two open findings stay open:
    finding `auto_sizer_short_push_empty` and finding
    `searched_doppler_unreported`.

### 7.1 The speed rows, with their budgets

Every row is timed by `make bench-ab` on the bench box: A is main before the
sub-step, B its head, K = 5, per-row min-of-K. The unit is µs per call as
the bench reports it, and the number is A's min, written into the PR beside
B's. **The budget is no SLOWER verdict on any row**, where a verdict counts
only if the same row is flagged twice (step 2's rule). A row that slows
within budget is stated with its ratio, never left unsaid.

The rows, by family, as `benchmarks/published/` names them (the 3.1b rows
join once they land):

| sub-step | family                       | rows                                                                                                                                                                                                                                                          |
| -------- | ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 3.2      | `corr::execute`              | `[accumulate]` and `[dump]`, each at `[n=1024]` and `[n=16384]`; 3.1b's `[n=31]`, `[n=63]`, `[n=127]`                                                                                                                                                         |
| 3.2      | `corr2d::` single shapes     | `single_row_fast_path`, `multi_row_general_path`                                                                                                                                                                                                              |
| 3.2      | `corr2d::delay_doppler_`     | `32x1024_1t`, `256x4096_1t`, `256x4096_all`                                                                                                                                                                                                                   |
| 3.2      | `corr2d::batch_64x512_h48_`  | `execute_loop_1t`, `all_1t`, `power_max_1t`, `execute_loop_all`, `all_all`, `power_max_all`                                                                                                                                                                   |
| 3.2      | `corr2d::bank_64x512_`       | `c12_corrected_batch_1t`, `c12_l4_1t`, `c6_l2_1t`, `detect_l2_l4_1t`                                                                                                                                                                                          |
| 3.3      | `detector::push`             | `[chunk=64]`, `[chunk=1024]`, `[chunk=16384]`, `[chunk=1024,no-detections]`; 3.1b's `[frame=31]`, `[frame=63]`, `[frame=127]`                                                                                                                                 |
| 3.3      | `detector2d::push`           | `[16x1024]`, `[128x128]`, `[1024x16]`, `[128x128,no-detections]`                                                                                                                                                                                              |
| 3.3      | `carrier_acq::detect`        | `[pad=4]`, `[pad=16]`                                                                                                                                                                                                                                         |
| 3.4      | `acq::`                      | `native_nc90`, `op5M_U50k_nc254`, `op2M_U50k_nc57`, `op5M_U5k_nc225`, `op2M_U5k_nc51`, `op5M_U50k_t8_nc254`, `op5M_U50k_D150_t1_nc1`, `op5M_U50k_D150_t8_nc1`, `op2M_U50k_D60_t1_nc1`, `op2M_U50k_D60_t4_nc1`, `op5M_U5k_D150_t1_nc1`, `op5M_U5k_D150_t4_nc1` |
| 3.4      | `async_dsss_pool::push`      | `[native,n=1,threads=1]`, `[native,n=4,threads=1]`, `[native,n=4,threads=4]`, `[full,n=1,threads=1]`, `[full,n=4,threads=1]`                                                                                                                                  |
| 3.4      | `async_dsss_receiver::steps` | `[track,sf=1023,3.069Mcps]`, `[search,sf=1023,3.069Mcps]`                                                                                                                                                                                                     |
| 3.4      | `burst_capture::push`        | `[4 bursts]`                                                                                                                                                                                                                                                  |

**The family names stay through step 3, except the one jm renames.** The
benches keep writing `corr`, `detector`, `detector2d` and `acq`, so
`bench-ab` pairs each row on main with the same row on the sub-step's head.
The 2-D family is renamed in 3.2, `corr2d` to `correlator`, because jm
collects a component's bench by its name, and it is judged with
`make bench-ab ... ALIAS=bench_corr2d_core=bench_correlator_core` (#2884,
#2889: bench-ab keys a row by its fullname, whose family is the bench
binary), which pairs the rows suffix for suffix and fails any that do not
pair. Step 4 renames the rest,
once, to `correlator::` with each row's flavor in its brackets, with the same
alias, and relabels them in `benchmarks/labels.toml`.

acq's carrier and five-tile rows from step 2 (`op5M_U10k_D150_t1`, the
`_fc` rows) are on main's bench but not yet in a published set; 3.4's PR
names them with the rest.

## 8. Not in step 3

- The Python `Correlator` class, and retiring `Acquisition`,
    `BurstAcquisition` and the `burst_acq` forwarder: step 4.
- Folding `PolynomialPhaseEstimator` in, and renaming `CarrierAcquisition`
    to `CarrierEstimator`: they gate on their own checks (taxonomy §6.4).
