<!-- domain: acquire; summary: step 3 of the correlator consolidation (#2752): the seven correlation objects become one C object, correlation/correlator/, its layers and constructors, the duplications it removes, the unknowns, and the sub-steps with their oracles and speed gates -->

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
1. **Speed only goes up.** Every pinned benchmark row of every retired object
    is equal or faster on the bench box, judged by `make bench-ab`.

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

Nothing else in `native/` correlates by FFT: `ppe_core.c` is a periodogram
and `carrier_acq` already composes `CorrDetector`.

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

### 2.1 The 1-D pair is the 2-D pair at one row, bit for bit

A throwaway Python prototype (not committed) ran each 1-D object against its
2-D twin built with `ny = 1`:

- **`Corr` against `Corr2D`:** 32 maps over n = 31, 64, 100 and 1023, dwells
    1 and 3, lag pads 1 and 2. Every map is byte-identical.
- **`CorrDetector` against `CorrDetector2D`:** 110 hits over 48 streams (the
    four noise modes, dwells 1 and 2, n = 31, 64 and 127, pushed in odd
    chunks). Every hit is identical: `(lag, peak, noise, stat)` against
    `(0, col, peak, noise, stat)`.

The arithmetic agrees by construction. `dp_corr_execute`
(`corr_core.c:154-194`) and `Corr2D`'s single-row path (`corr2d_row`,
`corr2d_row_inverse`) take the same forward transform, the same
multiply-accumulate against the conjugated reference spectrum, the same
`dp_fft_pad_spectrum` and the same `1/n` after the inverse. So `corr/` is a
second implementation of a path `corr2d/` already has, and `detector/` a
second pipeline over it.

The prototype's ties were not reached: an all-zero stream emits no hit, so
it never tested which of two equal cells wins. `CorrDetector` takes the
lowest index with a strict `>` (`detector_core.c:49-52`). The sub-step that
deletes it pins that order on the replacing mode in C.

## 3. What is duplicated, which rule 1 removes

|        | the copies                                                                                                                                                                                                               | becomes                                                                                            |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------- |
| **D1** | `Corr`'s 1-D correlation (`corr_core.c:154-194`) and `Corr2D`'s single-row path (`corr2d_core.c:441-454`)                                                                                                                | the single-row path, at `ny = 1` (§2.1)                                                            |
| **D2** | `CorrDetector`'s argmax loop (`detector_core.c:49-52`) and `det_peak_list`                                                                                                                                               | `det_peak_list` with one peak                                                                      |
| **D3** | `Corr2D`'s own peak passes: `corr2d_fold_peaks` (`corr2d_core.c:1072-1106`) and the bank's 3 × 3 local maximum (`corr2d_core.c:1771-1808`), beside `det_peak_scan` and `det_peak_zone`                                   | one peak primitive in the correlator's private header, if the measurements agree (U3)              |
| **D4** | the dwell counter, `++count == dwell` (`corr_core.c:160`, `corr2d_core.c:620`, `:738`)                                                                                                                                   | the single-row path's, with D1                                                                     |
| **D5** | acq's setup-time n-point transform pairs outside the map: `acq_shape_of_template`, `acq_off_peak`, `acq_doppler_sidelobe` and `dp_acq_sidelobe_ratio` (`acq_core.c:1912-1913`, `:1975-1976`, `:2041-2042`, `:3009-3011`) | whichever of them is a correlation of the template runs on the 1-D shape; a periodogram stays (U4) |

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

**One state blob.** One magic, `CORL`, and one version. The flavor is the
blob's first field and a reject key, like the grid today. The payload is
each layer's own fields, packed by that layer once. The format is unreleased
(`docs/design/state-serialization.md`), so the old magics retire.

## 5. Goals, and what is not known yet

**Goals:** rule 1 and rule 2, every retired object's C tests passing against
its replacing flavor, and the certified reports (Corr2D, CorrDetector2D,
Acquisition, BurstAcquisition) re-certified on the one object with no limit
lost.

**Unknown, and measured in the sub-step that needs it:**

- **U1. The 1-D rows through the single-row path.** §2.1 shows the bits
    agree. Whether `Corr`'s benchmark rows (`bench_corr_core.c`, `[n=…]`) and
    `CorrDetector`'s (`push[chunk=…]`) stay as fast through `Corr2D`'s
    per-worker plans and pool dispatch is not known. They are short, and
    dispatch may dominate them.
- **U2. Ties in the one peak picker.** Whether `det_peak_list` with one peak
    keeps `CorrDetector`'s lowest-index order on equal cells.
- **U3. The bank's peaks on `det_peak_*`.** Whether `corr2d_fold_peaks` and
    the bank's local maximum are `det_peak_scan` / `det_peak_zone` under
    other names, or a different rule (a 3 × 3 window where acq zones by
    span). If different, the difference is stated, and one rule is chosen with
    the owner.
- **U4. acq's setup transforms.** Which of the four pairs in D5 are
    correlations of the template with itself, and whether the 1-D shape is
    as fast for a one-shot setup cost.

## 6. Decided (owner, 2026-11-05)

- **One hit record in C.** `det_result_t` (lag), `det_result2d_t` (row,
    col) and `acq_result_t` (row, col and the search's fields) are three
    records for one event. The correlator has one: `acq_result_t`'s fields
    under the correlator's name, with `row = 0` at one row. The C callers
    change in the sub-step that retires each record (`carrier_acq` is the
    only one outside the family).
- **The Python classes until step 4 are views of one jm object.** Step 4
    makes `Correlator` the one class, and until then each of today's classes
    keeps working. One jm object, `correlator`, carries today's classes as
    `[[correlator.views]]`: each view has its own `class_name`, `create_fn`
    and `init_params`, and `exclude_methods` trims what a flavor does not
    serve, as `ddc` does for `MatchedDDC`. A view shares its parent's
    methods verbatim, so `push` returns the one record:
    `CorrDetector.push` returns the 2-D record (row 0, then the lag as the
    column) from sub-step 3.3, one step before step 4. That sub-step's
    changelog entry says so.

## 7. The sub-steps

Each is its own PR, held to its oracle and speed rows, and lands only with
CI green and the A/B run on the bench box.

1. **3.1, this page.**
1. **3.2, the map.** `correlator/` is created from `corr2d/`; `Corr` becomes
    the single-row flavor; `corr/` and `corr2d/` are deleted (D1, D4).
    - Oracle: `test_corr_core.c` and `test_corr2d_core.c`, run against the
        flavors.
    - Speed: `corr::*`, `corr2d::*`.
1. **3.3, detection.** `CorrDetector` and `CorrDetector2D` become the
    detector flavors; `det_private.h` becomes the correlator's private
    header; `detector/` and `detector2d/` are deleted (D2, D3).
    `carrier_acq` composes the 1-D detector flavor.
    - Oracle: `test_detector_core.c`, `test_detector2d_core.c` and
        `carrier_acq`'s tests. U2 is pinned in C.
    - Speed: `detector::*`, `detector2d::*`, `carrier_acq::*`.
1. **3.4, the search.** acq's engine moves in as the two search flavors and
    `acq/` is deleted; D5's correlations run on the 1-D shape.
    `BurstCapture`, the receivers and the pool call `dp_correlator_*`;
    `burst_acq` forwards to the new names until step 4.
    - Oracle: `test_acq_core.c`, `test_acq_regrid_failure.c`, and every
        acq validator's `--check` verdict unchanged.
    - Speed: `acq::*`, `async_dsss_pool::*`, `burst_capture::*`.
1. **3.5, re-certification.** One report for the one object, its limits
    the union of today's four reports. acq's two open findings stay open:
    finding `auto_sizer_short_push_empty` and finding
    `searched_doppler_unreported`.

## 8. Not in step 3

- The Python `Correlator` class, and retiring `Acquisition`,
    `BurstAcquisition` and the `burst_acq` forwarder: step 4.
- Folding `PolynomialPhaseEstimator` in, and renaming `CarrierAcquisition`
    to `CarrierEstimator`: they gate on their own checks (taxonomy §6.4).
- DelayDoppler's guide and gallery pages: step 4, written for `Correlator`.
