"""Phase 0 of the coupled-tracker roadmap
(`~/.claude/plans/jiggly-munching-newell.md`): formalize the
Acquisition -> tracking handoff, prototyped here in Python first (per
this project's own "validate risky DSP redesigns in Python with
doppler's real objects before writing new C" discipline) against the
REAL `doppler.dsss.Acquisition` C object -- not a hand-rolled sweep
script (`archive/acq_reps_noncoh_sweep.py`'s vintage), now that
wideband mode is genuinely wired into `acq_core.c` (task #72).

Every earlier script in this folder (`pullin_sweep.py`,
`characterize_snr.py`, `doppler_rate_test.py`) seeds
`CoupledAsyncDespreader` from a HAND-CHOSEN `seed_gap_hz` -- a stand-in
for "whatever Acquisition would have said," calibrated against this
story's own measured Acquisition-isolation statistics (mean |error|
~685 Hz, worst case 1320+ Hz). This module replaces that stand-in with
the real conversion: a genuine `acq.push()` hit -> a seed the tracker
can actually be constructed with.

**The conversion (`handoff_from_hit`)**:

- `acq_result_t::doppler_bin` composes against `acq.doppler_bins`
  (`Acquisition` always window-tiles now -- see `SPEC.md`'s
  Acquisition/BurstAcquisition split and `acq_core.h`'s file doc
  comment): fold it into a centered, signed hypothesis index
  (`0..doppler_bins/2` -> itself; beyond that -> `- doppler_bins`,
  matching the native FFT-bin convention already used for the
  slow-time axis) and scale by `doppler_res_hz` (chip_rate/(sf*
  doppler_bins)) to get a coarse Hz estimate. One formula, no
  wideband/native mode branch -- continuous `Acquisition` no longer
  has two modes to distinguish.
- `acq_result_t::code_phase` is a RAW SAMPLE index into the
  `code_bins = sf*spc` epoch; dividing by `spc` converts it to the
  CHIP-unit `chip_phase` `DetectionEvent` reports, in `Acquisition`'s
  own native convention (`acq_core.h`: `burst[q] = replica[(q-d) mod
  nx]`).
- `cn0_dbhz_est`, `peak_mag`, `noise_est`, `test_stat` pass straight
  through as diagnostics (`cn0_dbhz_est` not yet used to size
  `fll_block_epochs` -- see the module's "Not yet decided" note below).

**Found validating against `SPEC.md`'s real operating point (this
session): `CoupledAsyncDespreader.init_chip`'s own convention is the
MIRROR IMAGE of `Acquisition`'s (`tracker_init_chip = (sf -
chip_phase) % sf`, not `chip_phase` directly) -- a real, previously
undiscovered handoff bug, not a code-phase-in-general sensitivity.**
Every earlier script in this folder always collected/tracked at code
phase 0, so this convention mismatch had zero surface area to show up
until a genuine, nonzero `Acquisition`-detected phase was fed through
for the first time. `code_rate` staying pinned near 1.0 regardless of
whether the phase is right or wrong does NOT catch this -- it only
reflects the loop filter's own rate integrator, not absolute phase
lock, so it looked deceptively healthy even while completely
mismatched. `main()` computes the flip once (`tracker_init_chip`) and
threads it into both `refine_seed()` and the final tracking
`CoupledAsyncDespreader` construction. `freq_refine.refine_seed()`
also gained an `init_chip` parameter (previously always defaulted its
own collection tracker to phase 0, silently ignoring any real
detected phase -- invisible for the same reason). This is specific to
this Python prototype's own `CoupledAsyncDespreader`; the shipped C
`DsssReceiver` (`dsss_receiver_core.c`) already bridges `Acquisition`
into `Dll` correctly, confirmed by its own passing decode tests.

`DetectionEvent` is `SPEC.md`'s finalized output shape, field-for-field
-- the same record `dsss_acq_handoff_from_result()` will build in C.
Grid-relative diagnostics that only make sense alongside the emitting
`Acquisition`'s own config (`doppler_bin`, `n_hyp`, `wideband`) are
computed locally inside `handoff_from_hit()` but deliberately don't
survive into the record itself; a caller that wants
`init_car_norm_freq` divides `doppler_hz_est` by its own known sample
rate (see `main()`), rather than the record carrying a cycles/sample
value that's meaningless without that same rate shipped alongside it.

**Why a refine step still follows the handoff, not instead of it**:
wideband mode's `doppler_res_hz` is the WINDOW spacing, not a fine
estimate within it -- the true residual can be anywhere in
`+-doppler_res_hz/2` around the coarse Hz estimate. For this story's
own `SPEC.md` waveform that is +-1500 Hz, comfortably inside
`freq_refine.py`'s already-characterized working range (locks up to
the 1320 Hz worst case `pullin_sweep.py` measured for the OLD
non-wideband auto-config) -- so the existing bridge composes with the
new wideband handoff with NO changes needed to `freq_refine.py`
itself, only a real seed feeding it instead of a hand-chosen gap.

**Not yet decided (flagged, not solved here)**: `fll_block_epochs`
sizing from `cn0_dbhz_est` (the design rule from Phase 1e: size to the
SHORTEST block that stays reliable at the estimated Es/N0 floor, per
`characterize_snr.py`'s own sweep) -- this module reports
`cn0_dbhz_est` but leaves the caller to choose `fll_block_epochs`
explicitly for now, rather than guessing an interpolation off
`characterize_snr.py`'s few sampled points.

**Timing (`timestamp_ns`)**: `Acquisition.push()` now reports
`samples_consumed` per hit (task #71/timestamp-mechanism follow-on) --
the exact raw-sample anchor a `doppler.wfm.SampleClock` needs to derive
a precise per-hit wall-clock timestamp via `stamp_at()`, instead of
reusing one message-level timestamp for every hit a `push()` call might
emit. `handoff_from_hit()` takes an optional `clock` parameter; when
given, `DetectionEvent.timestamp_ns = clock.stamp_at(samples_consumed)`.
`search_and_handoff()` no longer reconstructs `samples_consumed`
manually from a local frame counter -- it reads the real value straight
off the hit (and cross-checks it against its own frame-counting loop as
a regression guard, since both should always agree).

Run: `python acq_handoff.py` (needs numpy + doppler) for a full
search -> handoff -> refine -> track demonstration against a real
`Acquisition` object at `SPEC.md`'s ACTUAL operating point, not a
scaled-down placeholder: the full +/-50 kHz uncertainty (the real
34-bin wideband grid `bench_acq_core.c` benchmarks), the exact CCSDS
Gold-1023 @ 3.069 Mcps / async BPSK @ 2700 bps waveform, `cn0_dbhz`
solved for the Es/N0=3 dB worst-case floor exactly, and `bn<=0.01` on
every tracking loop per `SPEC.md`'s derived loop-bandwidth rule.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from despreader_coupled import CoupledAsyncDespreader
from freq_refine import refine_seed
from signal_gen import SF, SPS, TE, signal

from doppler.dsss import Acquisition
from doppler.wfm import Gold, SampleClock

# `signal_gen`'s SF/SPS/TE are already SPEC.md-compatible in sample-domain
# terms (sf=1023, spc=2 -> code_bins=2046, matching bench_acq_core.c
# exactly) -- only its own CHIP_RATE (2.046 Mcps, a generic placeholder
# geometry shared by this folder's earlier phase-characterization
# scripts) needs overriding here with SPEC.md's real waveform numbers.
# Neither CHIP_RATE nor SYMBOL_RATE enters signal_gen.signal()'s own math
# directly (it's purely sample/normalized-frequency-domain), so this
# override is sufficient without touching signal_gen.py itself.
CHIP_RATE = 3.069e6  # SPEC.md: CCSDS Command link Gold Code 1023 @ 3.069 Mcps
SYMBOL_RATE = 2700.0  # SPEC.md: Asynchronous Rectangular BPSK @ 2700 bps
SAMPLE_RATE_HZ = CHIP_RATE * SPS
CARRIER_FREQ_HZ = 2.5e9  # SPEC.md: nominal frequency 2.5 GHz
DOPPLER_UNCERTAINTY_HZ = 50_000.0  # SPEC.md: +/-50 kHz -- the FULL 34-bin
# wideband grid (native span = chip_rate/sf = 3.000 kHz; ceil(100000/3000)
# = 34 windows spanning the full +/-50kHz), i.e. bench_acq_core.c's own
# operating point, not a scaled-down placeholder.

# SPEC.md's derived rule: bn <= 0.01 for EVERY tracking loop (code DLL,
# Costas carrier, FLL-assist alike -- the loop-SNR>=20dB-at-the-floor
# relation depends only on Es/N0 and bn, not which loop it is). Passed
# explicitly below rather than relying on CoupledAsyncDespreader's own
# bn_car=10*bn default, which would silently exceed this ceiling.
BN_CODE = 0.01
BN_CAR = 0.01

# Es/N0(dB) = cn0_dbhz - 10*log10(symbol_rate); solve for cn0_dbhz at
# SPEC.md's Es/N0 = 3 dB worst-case floor exactly -- matches
# bench_acq_core.c's own CN0_DBHZ=37.31 constant.
ESN0_FLOOR_DB = 3.0
CN0_DBHZ_FLOOR = ESN0_FLOOR_DB + 10.0 * np.log10(SYMBOL_RATE)


@dataclass(frozen=True)
class DetectionEvent:
    """`SPEC.md`'s finalized output shape, field-for-field (the same
    record `dsss_acq_handoff_from_result()` will build in C) -- a flat,
    pointer-free POD safe to serialize across a process boundary."""

    timestamp_ns: int | None  # clock.stamp_at(samples_consumed); None if
    # no clock was supplied to handoff_from_hit()
    samples_consumed: int  # acq_result_t's own per-hit raw-sample anchor
    chip_phase: float  # chips -- init_chip seed
    doppler_hz_est: float  # coarse Hz estimate, folded/signed/scaled
    doppler_res_hz: float  # width of that estimate (+-doppler_res_hz/2
    # remaining uncertainty a downstream refine/tracking stage must close)
    cn0_dbhz_est: float
    peak_mag: float  # raw CFAR peak magnitude -- diagnostic passthrough
    noise_est: float  # raw CFAR noise-floor estimate -- diagnostic passthrough
    test_stat: float  # raw CFAR gating statistic -- diagnostic passthrough


def handoff_from_hit(acq, hit, spc, sample_rate_hz, clock=None):
    """Convert one `acq.push()` hit tuple (`doppler_bin, code_phase,
    peak_mag, noise_est, test_stat, cn0_dbhz_est, samples_consumed`)
    into a `DetectionEvent`. `acq` must be the SAME `Acquisition` object
    the hit came from (its `n_freq_bins`/`doppler_bins`/`doppler_res_hz`
    describe the grid the hit's indices are relative to).

    `clock` is an optional `doppler.wfm.SampleClock` already anchored
    (via `track()`) against this stream's own incoming message headers
    -- when given, `timestamp_ns = clock.stamp_at(samples_consumed)`
    gives this specific hit's precise wall-clock time, not "whichever
    message happened to be current." `None` leaves `timestamp_ns` unset
    (e.g. a purely synthetic run with no real wall clock to anchor to).
    """
    (
        doppler_bin,
        code_phase,
        peak_mag,
        noise_est,
        test_stat,
        cn0_est,
        samples_consumed,
    ) = hit
    n_hyp = acq.doppler_bins
    signed = doppler_bin if doppler_bin <= n_hyp // 2 else doppler_bin - n_hyp
    doppler_hz_est = signed * acq.doppler_res_hz
    timestamp_ns = (
        clock.stamp_at(samples_consumed) if clock is not None else None
    )
    return DetectionEvent(
        timestamp_ns=timestamp_ns,
        samples_consumed=samples_consumed,
        chip_phase=code_phase / spc,
        doppler_hz_est=doppler_hz_est,
        doppler_res_hz=acq.doppler_res_hz,
        cn0_dbhz_est=cn0_est,
        peak_mag=peak_mag,
        noise_est=noise_est,
        test_stat=test_stat,
    )


def search_and_handoff(acq, rx, spc, sample_rate_hz, clock=None):
    """Feed `rx` into `acq` one epoch at a time until the first hit,
    then return `(DetectionEvent, samples_consumed)`. `samples_consumed`
    comes straight off the hit itself (`acq_result_t`'s own per-hit
    anchor, task #71's timestamp follow-on) -- the loop below also
    tracks it independently from the epoch-feed count as a cheap
    regression guard (the two must always agree), not because the real
    value is unavailable any more.

    Raises `RuntimeError` if `rx` runs out before any hit fires.
    """
    frame_n = acq.code_bins
    consumed = 0
    while consumed + frame_n <= len(rx):
        chunk = rx[consumed : consumed + frame_n].astype(np.complex64)
        hits = acq.push(chunk)
        consumed += frame_n
        if hits:
            event = handoff_from_hit(acq, hits[0], spc, sample_rate_hz, clock)
            assert event.samples_consumed == consumed, (
                f"acq_result_t.samples_consumed ({event.samples_consumed}) "
                f"disagrees with the epoch-feed count ({consumed})"
            )
            return event, event.samples_consumed
    raise RuntimeError(
        f"no acquisition hit in {len(rx)} samples ({consumed} consumed)"
    )


# characterize_snr.py's own Es/N0 sweep found `found_right_peak` at its
# lowest characterized point (2 dB, 1 dB below this exact floor) needs up
# to ~2700 epochs to reach 0.83-0.92 -- nowhere near the old placeholder's
# 300. Sized to that same 2700-epoch collection (doppler_rate_test.py's
# own "Phase-1c-validated static window" length) since this floor is only
# 1 dB better.
REFINE_PREFIX_EPOCHS = 2700

# Enough data symbols to cover the acquisition search dwell (bench_acq_core.c
# measures n_noncoh~112 epochs at this exact cn0/pd=0.9 default) +
# REFINE_PREFIX_EPOCHS + a healthy tracking tail, at
# epochs_per_symbol = (chip_rate/sf)/symbol_rate ~= 1.11 epochs/symbol.
N_SYM = 8000


def main():
    c = np.asarray(Gold().generate(SF)).astype(np.uint8)
    true_doppler_hz = 45_000.0  # deep inside +/-50kHz -- exercises the
    # real 34-bin wideband search across most of its span, not a narrow
    # slice near DC.
    cn0_dbhz = CN0_DBHZ_FLOOR
    # signal_gen.signal()'s own snr_db is a per-raw-sample power ratio,
    # not cn0_dbhz/Es_N0 directly -- inverse of Acquisition's own sizing
    # transform (amp_snr = sqrt(10**(cn0_dbhz/10) / fs)).
    chip_snr_db = cn0_dbhz - 10.0 * np.log10(SAMPLE_RATE_HZ)
    symbol_offset = 0.37 * TE  # a symbol-BOUNDARY offset -- signal_gen's
    # own `cph = (idx // SPS) % SF` never depends on `phi`, so this does
    # NOT move the PN code phase (it's always 0-aligned to idx=0 by that
    # generator's construction). A separate raw-sample delay below is
    # what actually exercises a nonzero code-phase handoff.
    code_phase_delay = 757  # raw samples dropped off the front below.

    epochs_per_symbol = (1.0 / SYMBOL_RATE) / (SF / CHIP_RATE)
    rx, _data, _tsym = signal(
        c,
        N_SYM,
        epochs_per_symbol,
        symbol_offset,
        true_doppler_hz / SAMPLE_RATE_HZ,
        chip_snr_db,
        5,
    )
    rx = rx[code_phase_delay:]
    # Dropping the first `code_phase_delay` raw samples is equivalent to
    # ADVANCING the code phase by that amount, i.e. `acq_core.h`'s own
    # "code_phase = d" convention (burst[q] = replica[(q - d) mod nx],
    # matching `_acq_wideband_check`'s C test) sees the negative: true
    # code_phase = (-code_phase_delay) mod TE, in chips = that / SPS.
    true_init_chip = ((-code_phase_delay) % TE) / SPS

    acq = Acquisition(
        c,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        pfa=1e-3,
        pd=0.9,
        symbol_rate=SYMBOL_RATE,
    )
    print(
        f"--- Acquisition: doppler_bins={acq.doppler_bins} "
        f"n_noncoh={acq.n_noncoh} doppler_res_hz={acq.doppler_res_hz:.1f} "
        f"cn0_dbhz={cn0_dbhz:.1f} ---"
    )

    # A receive-side SampleClock, anchored via track() against this
    # stream's own "arrival" (a purely synthetic run has no real wall
    # clock to anchor to, so this is a fixed, arbitrary epoch chosen only
    # to demonstrate the mechanism -- a real caller anchors track() from
    # an actually-incoming dp_header_t.timestamp_ns instead). Anchoring
    # at n=0 with this epoch means stamp_at(n) == epoch + n/fs exactly,
    # letting the printed value below be checked against that formula
    # directly.
    clock = SampleClock(SAMPLE_RATE_HZ)
    stream_epoch_ns = 1_700_000_000_000_000_000
    clock.track(stream_epoch_ns, 0, tolerance_ns=1_000_000)

    handoff, consumed = search_and_handoff(
        acq, rx, SPS, SAMPLE_RATE_HZ, clock=clock
    )
    coarse_err_hz = abs(handoff.doppler_hz_est - true_doppler_hz)
    chip_err = abs(handoff.chip_phase - true_init_chip)
    expected_ts = stream_epoch_ns + round(
        handoff.samples_consumed / SAMPLE_RATE_HZ * 1e9
    )
    assert handoff.timestamp_ns == expected_ts, (
        f"timestamp_ns {handoff.timestamp_ns} != expected {expected_ts}"
    )
    print(
        f"--- timing: samples_consumed={handoff.samples_consumed} "
        f"timestamp_ns={handoff.timestamp_ns} "
        f"(epoch + samples_consumed/fs, verified exact) ---"
    )
    print(
        f"--- handoff: chip_phase={handoff.chip_phase:.2f} "
        f"(true={true_init_chip:.2f}, err={chip_err:.2f} chips) "
        f"doppler_hz_est={handoff.doppler_hz_est:+.1f} "
        f"+-{handoff.doppler_res_hz / 2:.1f} "
        f"(true={true_doppler_hz:+.1f}, err={coarse_err_hz:.1f} Hz) "
        f"cn0_dbhz_est={handoff.cn0_dbhz_est:.1f} "
        f"consumed={consumed} samples ---"
    )

    # Acquisition's own code_phase convention (acq_core.h: burst[q] =
    # replica[(q-d) mod nx], validated exact above) is the MIRROR IMAGE of
    # CoupledAsyncDespreader's own init_chip convention -- found the hard
    # way validating against SPEC.md's real operating point (see the
    # module docstring): feeding handoff.chip_phase straight into
    # init_chip is silently wrong (code_rate stays ~1.0 regardless, since
    # it only reflects the loop's rate integrator, not absolute phase
    # lock -- it does NOT catch this). This flip is specific to this
    # prototype's OWN tracker; DsssReceiver's real C handoff
    # (dsss_receiver_core.c) already gets this right internally.
    tracker_init_chip = (SF - handoff.chip_phase) % SF

    coarse_norm_freq = handoff.doppler_hz_est / SAMPLE_RATE_HZ
    prefix = rx[consumed : consumed + REFINE_PREFIX_EPOCHS * TE]
    refined_norm_freq, residual_est_hz = refine_seed(
        CoupledAsyncDespreader,
        c,
        SPS,
        BN_CODE,
        coarse_norm_freq,
        prefix,
        SAMPLE_RATE_HZ,
        bn_car=BN_CAR,
        windows=6,
        init_chip=tracker_init_chip,
    )
    refined_hz = refined_norm_freq * SAMPLE_RATE_HZ
    refine_err_hz = abs(refined_hz - true_doppler_hz)
    print(
        f"--- refine: residual_est={residual_est_hz:+.1f} Hz "
        f"refined_hz={refined_hz:+.1f} (err={refine_err_hz:.1f} Hz) ---"
    )

    track_rx = rx[consumed + len(prefix) :]
    d = CoupledAsyncDespreader(
        c,
        SPS,
        bn=BN_CODE,
        zeta=0.707,
        windows=6,
        bn_car=BN_CAR,
        init_chip=tracker_init_chip,
        init_car_norm_freq=refined_norm_freq,
        aid_code=True,
        sample_rate_hz=SAMPLE_RATE_HZ,
        carrier_freq_hz=CARRIER_FREQ_HZ,
    )
    # Track in chunks and average the TAIL of car_norm_freq readings,
    # not a single final snapshot: at this chip_snr_db the closed loop's
    # steady-state output genuinely JITTERS (measured std ~10 Hz around
    # the true value, symmetric, non-diverging) rather than settling to
    # a fixed point -- a single instantaneous readout is measuring that
    # noise, not whether the loop is centered on the right frequency.
    # Averaging trailing readings is what a real receiver would report
    # as "current lock frequency" too.
    chunk = 100 * TE
    tail_readings = []
    n_chunks = min(len(track_rx) // chunk, 40)
    for i in range(n_chunks):
        d.run(track_rx[i * chunk : (i + 1) * chunk])
        tail_readings.append(d.car_norm_freq * SAMPLE_RATE_HZ)
    tail_mean_hz = float(np.mean(tail_readings[-20:]))
    final_err_hz = abs(tail_mean_hz - true_doppler_hz)
    locked = final_err_hz < 10.0 and abs(d.code_rate - 1.0) < 1e-4
    print(
        f"--- track: tail_mean_car_hz={tail_mean_hz:+.1f} "
        f"(err={final_err_hz:.2f} Hz, over last "
        f"{min(20, len(tail_readings))} of {n_chunks} x {chunk // TE} "
        f"epoch chunks) code_rate={d.code_rate:.6f} "
        f"last_error={d.last_error:+.5f} [{'LOCKED' if locked else 'FAILED'}]"
        " ---"
    )
    assert locked, "handoff -> refine -> track chain failed to lock end to end"
    print("\nEnd-to-end search -> handoff -> refine -> track: LOCKED.")


if __name__ == "__main__":
    main()
