"""Acquisition -> tracking handoff glue: converts a real
`doppler.dsss.Acquisition.push()` hit into a `DetectionEvent` seed a
tracker can be constructed from (`handoff_from_hit`), and drives that
search to the first hit (`search_and_handoff`). Reused by
`e2e_acq_to_despreader.py` (the folder's one canonical Acq->Despreader
end-to-end test) and by anything else in this folder that needs a real
handoff instead of a hand-chosen seed frequency.

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
  `bn_fll_car` -- see the module's "Not yet decided" note below).

**Found validating against `SPEC.md`'s real operating point:
`CoupledAsyncDespreader.init_chip`'s own convention is the MIRROR
IMAGE of `Acquisition`'s (`tracker_init_chip = (sf - chip_phase) %
sf`, not `chip_phase` directly) -- a real handoff bug, not a
code-phase-in-general sensitivity. `code_rate` staying pinned near 1.0
regardless of whether the phase is right or wrong does NOT catch this
-- it only reflects the loop filter's own rate integrator, not
absolute phase lock. Every caller (`e2e_acq_to_despreader.py`,
`freq_refine.refine_seed`/`refine_seed_matched`) must apply this flip
before constructing/seeding a tracker. This is specific to this Python
prototype's own `CoupledAsyncDespreader`; the shipped C `DsssReceiver`
(`dsss_receiver_core.c`) already bridges `Acquisition` into `Dll`
correctly, confirmed by its own passing decode tests.

`DetectionEvent` is `SPEC.md`'s finalized output shape, field-for-field
-- the same record `dsss_acq_handoff_from_result()` will build in C.
Grid-relative diagnostics that only make sense alongside the emitting
`Acquisition`'s own config (`doppler_bin`, `n_hyp`, `wideband`) are
computed locally inside `handoff_from_hit()` but deliberately don't
survive into the record itself; a caller that wants
`init_car_norm_freq` divides `doppler_hz_est` by its own known sample
rate.

**Why a refine step still follows the handoff, not instead of it**:
wideband mode's `doppler_res_hz` is the WINDOW spacing, not a fine
estimate within it -- the true residual can be anywhere in
`+-doppler_res_hz/2` around the coarse Hz estimate, so
`freq_refine.py`'s PSDMF bridge still runs afterward to close that gap
before tracking starts.

**Timing (`timestamp_ns`)**: `Acquisition.push()` reports
`samples_consumed` per hit -- the exact raw-sample anchor a
`doppler.wfm.SampleClock` needs to derive a precise per-hit wall-clock
timestamp via `stamp_at()`. `handoff_from_hit()` takes an optional
`clock` parameter; when given, `DetectionEvent.timestamp_ns =
clock.stamp_at(samples_consumed)`. `search_and_handoff()` cross-checks
`samples_consumed` against its own frame-counting loop as a regression
guard (the two must always agree).
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


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
