"""Resample-then-track carrier loop: mirrors `~/legacy-commz`'s actual
carrier-tracking architecture (`Despreader.step()`'s own
`self.resampler` -> `self.carrier_loop` -> `_generate_carrier_control`
chain), which this session's #102 `car_update_windows` mode does NOT
match -- that mode discriminates directly off raw, un-filtered
per-window `integrate_and_dump` partials, at whatever raw `windows`
granularity the CODE loop happens to use (chosen for the CODE loop's
own async-data-boundary/Nyquist needs, not for carrier-loop SNR per
update). Found via a direct code comparison against the reference
(`FINISHING_PLAN.md`/project memory, CHECKPOINT 18's divergence list)
after confirming BOTH the old and new architectures fail to lock
(BER~0.47-0.5) on a real `~/legacy-commz`-generated capture despite an
accurate PSDMF seed -- this is the top untested candidate for why.

The reference instead:
  1. resamples (anti-alias filtered, real FIR `Resampler`) the raw
     windows-rate despread stream DOWN to a much coarser rate --
     specifically the SAME rate the downstream demod runs at
     (`Despreader.output_sample_rate` is passed straight through as
     `bpsk.Receiver`'s own `sample_rate`, `receiver.py`),
  2. runs ONE Costas-style discriminator + loop-filter update per
     RESAMPLED sample (not per raw window),
  3. zero-order-holds (repeats) that correction back up to raw-window
     granularity for the next epoch's carrier NCO ctrl port
     (`_generate_carrier_control`'s `np.repeat`).

Built as an EXTERNAL harness around `CoupledAsyncDespreader`
(`car_update_windows=True`, `freeze_carrier=True`) rather than a new
mode inside that class: `freeze_carrier=True` already disables the
class's own internal per-window discriminator/loop-filter update
while leaving `_car_ctrl_windows` (and the derotation that reads it)
fully functional -- see `despreader_coupled.py`'s own `run()`, the
`if not self.freeze_carrier:` guard around the per-window `car_lf`
update only, `self._car_rate_dev = self._car_ctrl_windows[-1]` stays
unconditional either way. This harness simply writes directly into
`_car_ctrl_windows` between `run()` calls instead of letting the
class compute it. Reuses `despreader_coupled`'s own `COSTAS_EPS`
convention (imported, not re-derived) so the discriminator formula
stays byte-identical to the rest of this project.
"""
from __future__ import annotations

import numpy as np

from despreader_coupled import COSTAS_EPS
from doppler.resample import RateConverter
from doppler.track import LoopFilter


class ResampleTrackedCarrier:
    """Persistent resample-then-track carrier-loop state, driven
    externally by one epoch's raw per-window despread partials at a
    time (`update(out_epoch)`), returning the per-fs_front-sample
    deviation to hold for the NEXT epoch's `CoupledAsyncDespreader
    ._car_ctrl_windows` (a `CoupledAsyncDespreader` constructed with
    `car_update_windows=True, freeze_carrier=True` -- see module
    docstring for why that combination is the right hook).

    Parameters
    ----------
    partial_rate_hz : float
        Sample rate of the raw per-window stream fed to `update`
        (`chip_rate * windows / sf`, the SAME `windows` the
        `CoupledAsyncDespreader` instance was constructed with).
    target_rate_hz : float
        Rate to resample DOWN to before discriminating -- pick the
        SAME rate the downstream demod runs at, matching the
        reference's own choice (see module docstring).
    fs_front_hz : float
        The raw chip-rate-domain sample rate (`chip_rate * spc`) the
        resulting deviation must be expressed in, to feed
        `_car_ctrl_windows` (which `CoupledAsyncDespreader` multiplies
        against fs_front-domain phase advance via `np.repeat` +
        `steps_ctrl`).
    bn_car : float
        Loop filter bandwidth, interpreted at `target_rate_hz` (one
        `LoopFilter.step()` call per RESAMPLED sample) -- NOT the
        raw-window rate this project's other `bn_car` values assume;
        size it like a normal downstream Costas loop bandwidth (e.g.
        `MpskReceiver`'s own `bn_carrier` convention), since it now
        runs at essentially that same rate.
    """

    def __init__(
        self, partial_rate_hz, target_rate_hz, fs_front_hz, bn_car,
        zeta=0.707,
    ):
        self._rc = RateConverter(rate=target_rate_hz / partial_rate_hz)
        self._lf = LoopFilter(bn=bn_car, zeta=zeta, t=1.0)
        # cycles/low-rate-sample -> cycles/fs_front-sample: a low-rate
        # sample spans (fs_front_hz/target_rate_hz) fs_front samples,
        # so dividing by that many samples is multiplying by the
        # inverse ratio.
        self._scale = target_rate_hz / fs_front_hz
        self._dev = 0.0
        self.last_error = 0.0
        self.n_lowrate_samples = 0

    def update(self, out_epoch):
        """Feed one epoch's raw per-window despread partials (natural
        order, `windows`-length complex array, e.g. `d.run(seg)`'s own
        return value). Returns `(low, dev)`: `low` is this epoch's
        share of the SAME resampled stream the reference reuses for
        its downstream demod too (`Despreader.out` IS `self.resampler
        .out`, `despreader.py`'s own `step()`) -- the caller should
        feed `low` onward to its own downstream demod chain instead of
        re-resampling `out_epoch` a second time, to stay faithful to
        that one-resample-stream design. `dev` is the
        per-fs_front-sample carrier deviation to hold for the NEXT
        epoch (zero-order-held across whatever raw windows fall
        between resampled-rate updates, mirroring the reference's own
        `_generate_carrier_control` repeat)."""
        low = self._rc.execute(out_epoch.astype(np.complex64))
        for z in low:
            re, im = float(z.real), float(z.imag)
            a = abs(z) + COSTAS_EPS
            e = (im if re >= 0.0 else -im) / a
            self.last_error = e
            lf_out = self._lf.step(e)
            self._dev = lf_out / (2.0 * np.pi) * self._scale
        self.n_lowrate_samples += len(low)
        return low, self._dev
