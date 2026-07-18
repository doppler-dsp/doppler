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

- `acq_result_t::doppler_bin` composes IDENTICALLY whether Acquisition
  is in native or wideband mode (`acq_core.h`'s own file doc comment):
  fold it into a centered, signed hypothesis index (`0..n_hyp/2` ->
  itself; beyond that -> `- n_hyp`, matching the native FFT-bin
  convention already used for the slow-time axis) and scale by
  `doppler_res_hz` (chip_rate/sf in wideband mode; chip_rate/(sf*
  doppler_bins) natively) to get a coarse Hz estimate. `n_hyp` is
  `n_freq_bins` when wideband (`> 1`), else `doppler_bins` -- one
  formula, no mode branch needed beyond picking which count to fold
  over.
- `acq_result_t::code_phase` is a RAW SAMPLE index into the
  `code_bins = sf*spc` epoch; dividing by `spc` converts it to the
  CHIP-unit `init_chip` `CoupledAsyncDespreader` (and the shipped
  `Dll`) already expect.
- `cn0_dbhz_est` passes straight through as a diagnostic (not yet used
  to size `fll_block_epochs` -- see the module's "Not yet decided"
  note below).

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
`Acquisition` object at a modest wideband span (+-10 kHz, 10x this
geometry's 1 kHz native window -- large enough to force wideband mode
and exercise a real fold across the DC boundary, without re-measuring
`bench_acq_core.c`'s already-settled 34-bin/+-50kHz timing numbers).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from despreader_coupled import CoupledAsyncDespreader
from freq_refine import refine_seed
from signal_gen import CHIP_RATE, SF, SPS, TE, code, signal

from doppler.dsss import Acquisition
from doppler.wfm import SampleClock

SAMPLE_RATE_HZ = CHIP_RATE * SPS  # matches the class default (4.092e6)
CARRIER_FREQ_HZ = 2.2e9
DOPPLER_UNCERTAINTY_HZ = 10_000.0  # 10x the 1 kHz native window --
# forces wideband mode (n_freq_bins = ceil(10000/1000) = 10) without
# re-measuring bench_acq_core.c's own +-50kHz/34-bin numbers.


@dataclass(frozen=True)
class DetectionEvent:
    """The seed a tracker needs, plus the diagnostics behind it."""

    coarse_norm_freq: float  # cycles/sample -- init_car_norm_freq seed
    init_chip: float  # chips -- init_chip seed
    window_hz: float  # the coarse Hz estimate before scaling to
    # cycles/sample (diagnostic -- same information, Hz instead of
    # cycles/sample)
    cn0_dbhz_est: float
    doppler_bin: int
    n_hyp: int  # n_freq_bins if wideband else doppler_bins
    wideband: bool
    samples_consumed: int  # acq_result_t's own per-hit raw-sample anchor
    timestamp_ns: int | None  # clock.stamp_at(samples_consumed); None if
    # no clock was supplied to handoff_from_hit()


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
        _peak_mag,
        _noise_est,
        _test_stat,
        cn0_est,
        samples_consumed,
    ) = hit
    wideband = acq.n_freq_bins > 1
    n_hyp = acq.n_freq_bins if wideband else acq.doppler_bins
    signed = doppler_bin if doppler_bin <= n_hyp // 2 else doppler_bin - n_hyp
    window_hz = signed * acq.doppler_res_hz
    timestamp_ns = (
        clock.stamp_at(samples_consumed) if clock is not None else None
    )
    return DetectionEvent(
        coarse_norm_freq=window_hz / sample_rate_hz,
        init_chip=code_phase / spc,
        window_hz=window_hz,
        cn0_dbhz_est=cn0_est,
        doppler_bin=doppler_bin,
        n_hyp=n_hyp,
        wideband=wideband,
        samples_consumed=samples_consumed,
        timestamp_ns=timestamp_ns,
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
    frame_n = (
        acq.code_bins
        if acq.n_freq_bins > 1
        else acq.code_bins * acq.doppler_bins
    )
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


def _chip_snr_db_to_cn0_dbhz(chip_snr_db, sample_rate_hz):
    """Inverse of `Acquisition`'s own sizing transform (`amp_snr =
    sqrt(10**(cn0_dbhz/10) / fs)`), for a unit-chip-amplitude signal --
    the SAME per-sample SNR convention `signal_gen.signal()`'s
    `snr_db` argument already uses (see `characterize_snr.py`'s own
    documented derivation of this relationship, reused here in the
    other direction: `cn0_dbhz = chip_snr_db + 10*log10(fs)`)."""
    return chip_snr_db + 10.0 * np.log10(sample_rate_hz)


REFINE_PREFIX_EPOCHS = 300  # matches pullin_sweep.py's own validated
# default at a comparable operating point.


def main():
    c = code(11)
    true_doppler_hz = 6234.0  # inside +-10kHz, crosses window folds
    chip_snr_db = -8.0  # matches this suite's standard operating point
    cn0_dbhz = _chip_snr_db_to_cn0_dbhz(chip_snr_db, SAMPLE_RATE_HZ)
    symbol_offset = 0.37 * TE  # a symbol-BOUNDARY offset -- signal_gen's
    # own `cph = (idx // SPS) % SF` never depends on `phi`, so this does
    # NOT move the PN code phase (it's always 0-aligned to idx=0 by that
    # generator's construction). A separate raw-sample delay below is
    # what actually exercises a nonzero code-phase handoff.
    code_phase_delay = 757  # raw samples dropped off the front below.

    rx, _data, _tsym = signal(
        c,
        4000,
        (1.0 / 1800.0) / (SF / CHIP_RATE),
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
        reps=1,
        spc=SPS,
        chip_rate=CHIP_RATE,
        cn0_dbhz=cn0_dbhz,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        pfa=1e-3,
        pd=0.9,
        max_noncoh=100,
        symbol_rate=1800.0,
    )
    print(
        f"--- Acquisition: wideband={acq.n_freq_bins > 1} "
        f"n_freq_bins={acq.n_freq_bins} doppler_bins={acq.doppler_bins} "
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
    coarse_err_hz = abs(handoff.window_hz - true_doppler_hz)
    chip_err = abs(handoff.init_chip - true_init_chip)
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
        f"--- handoff: doppler_bin={handoff.doppler_bin}/"
        f"{handoff.n_hyp} code_phase_chips={handoff.init_chip:.2f} "
        f"(true={true_init_chip:.2f}, err={chip_err:.2f} chips) "
        f"window_hz={handoff.window_hz:+.1f} "
        f"(true={true_doppler_hz:+.1f}, err={coarse_err_hz:.1f} Hz) "
        f"cn0_dbhz_est={handoff.cn0_dbhz_est:.1f} "
        f"consumed={consumed} samples ---"
    )

    prefix = rx[consumed : consumed + REFINE_PREFIX_EPOCHS * TE]
    refined_norm_freq, residual_est_hz = refine_seed(
        CoupledAsyncDespreader,
        c,
        SPS,
        0.002,
        handoff.coarse_norm_freq,
        prefix,
        SAMPLE_RATE_HZ,
        bn_car=0.02,
        windows=6,
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
        bn=0.002,
        zeta=0.707,
        windows=6,
        bn_car=0.02,
        init_chip=handoff.init_chip,
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
