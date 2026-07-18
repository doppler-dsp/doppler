"""Same design as `despreader.py`, with code-phase tracking swapped from
a hand-rolled float accumulator to the real, C-backed `doppler.source.NCO`.

This is the first step of migrating the validated pure-Python prototype
to doppler's actual extension objects, one piece at a time (next up:
replica generation, then the loop filter), verifying bit-for-bit /
numerically-equivalent behaviour against the original pure-Python
version at each step -- see `compare_nco.py`, which runs both modules on
identical input and diffs their outputs and internal `code_rate`
trajectory.

`despreader.py` is kept unmodified as the pure-Python original for this
comparison; do not let the two drift apart except in the one piece being
swapped at each step.
"""
from __future__ import annotations

import numpy as np

from doppler.source import NCO


def replica2x(code, c):
    """2x-oversampled, dwell-center-correct code replica (matches the
    fixed `native/inc/dll/dll_core.h` `dll_replica`): the code is held at
    2 samples/chip, sampled at chip-relative positions {0.25, 0.75} (NOT
    {0, 0.5}, which confines the whole linear-interpolation transition to
    one side of each chip boundary), linearly interpolated at any
    fractional position. `c` is a numpy array of continuous chip
    positions (any real value, wrapped mod `len(code)`).
    """
    sf = len(code)
    sfd2 = 2.0 * sf
    p = (c * 2.0 - 0.5) % sfd2
    i = p.astype(np.int64)
    mu = p - i
    j = np.where(i + 1 >= sfd2, 0, i + 1)
    csign = np.where(code & 1, -1.0, 1.0)
    v0 = csign[(i >> 1) % sf]
    v1 = csign[(j >> 1) % sf]
    return (1.0 - mu) * v0 + mu * v1


def get_window(x, last_x, index):
    """Reconstruct a full-length array as [last `index` samples of the
    PREVIOUS buffer] + [all but the last `index` samples of the CURRENT
    buffer]. `index=0` is the identity (this epoch's own buffer,
    unshifted) -- the "natural window."""
    if index == 0:
        return x
    out = np.empty_like(x)
    out[:index] = last_x[-index:]
    out[index:] = x[:-index]
    return out


def find_max_power(x, windows, step_size, last_backward_sums):
    """Find the despread-output phase (window shift) with maximum
    correlation power, so a data transition inside the CURRENT epoch's
    buffer doesn't have to collapse the whole epoch's correlation: the
    lookback can instead reconstruct a transition-free window straddling
    this epoch and the previous one.

    `x` is this epoch's full despread-punctual product (signal * local
    punctual replica), length `windows * step_size`. `last_backward_sums`
    is the PREVIOUS epoch's `backward_sums` (or `None` for the very first
    epoch). Returns `(max_power, max_window, backward_sums,
    integrate_and_dump, window_index)` -- `window_index` (in samples) is
    what `get_window` needs to reconstruct the winning shift.
    """
    inv_buff_size = 1.0 / x.size
    partial_sums = x.reshape(windows, step_size).sum(axis=1)
    sums = partial_sums.cumsum()
    backward_sums = partial_sums[::-1].cumsum()

    correlations = np.empty(windows)
    correlations[-1] = np.abs(sums[-1]) * inv_buff_size
    if last_backward_sums is not None:
        correlations[:-1] = (
            np.abs(sums[:-1] + last_backward_sums[::-1][1:]) * inv_buff_size
        )
    else:
        correlations[:-1] = np.abs(sums[:-1]) * inv_buff_size

    max_window = int(correlations.argmax())
    max_abs = correlations[max_window]
    max_power = max_abs**2
    integrate_and_dump = partial_sums / (step_size * max_abs)
    window_index = (windows - 1 - max_window) * step_size

    return (
        max_power,
        max_window,
        backward_sums,
        integrate_and_dump,
        window_index,
    )


class LoopFilter:
    """Discrete-time, 2nd-order, type-2 PI loop filter (bilinear design,
    damping-and-bandwidth parameterized) matching the working
    implementation's `LoopFilter(design_method="discrete-time")` (its
    default) exactly -- NOT the "integrator + proportional-on-raw-error"
    form.

    `.out` is a *per-update-period* rate quantity: `bandwidth` is
    normalised to the filter's own call rate (once per epoch here), so
    the caller must divide `.out` by the number of samples in the
    upcoming period to get a per-sample rate. That division (not a
    separate, extra reduction of the proportional term alone) is the
    entire mechanism that keeps a single bad epoch's error from
    over-correcting -- see the module docstring point 2.
    """

    def __init__(self, bandwidth, damping=0.707):
        self.damping = damping
        self.kp, self.ki = self._coefficients(bandwidth)
        self.state = 0.0
        self.last_in = 0.0
        self.out = 0.0

    def _coefficients(self, bandwidth):
        d = self.damping
        a = -4 * d**2 * (2 * bandwidth + 3) / (2 * bandwidth + 1)
        b = (
            4 * d**2 * (8 * bandwidth + 2)
            + 16 * d**4 * (4 * bandwidth + 2)
        ) / (2 * bandwidth + 1)
        c = -128 * bandwidth * d**4 / (2 * bandwidth + 1)
        p = b - a**2 / 3
        q = 2 * a**3 / 27 - a * b / 3 + c
        disc = q**2 / 4 + p**3 / 27

        def cbrt(v):
            return v ** (1.0 / 3.0) if v >= 0 else -((-v) ** (1.0 / 3.0))

        kp = (
            cbrt(-q / 2 + np.sqrt(disc))
            + cbrt(-q / 2 - np.sqrt(disc))
            - a / 3
        )
        ki = kp**2 / (4 * d**2)
        return kp, ki

    def step(self, x):
        self.out = (
            self.state + self.kp * (x - self.last_in) + self.ki * self.last_in
        )
        self.state = self.out
        self.last_in = x
        return self.out


class SimpleAsyncDespreader:
    """Minimal, faithful port: NCO-driven 2x-oversampled code replica +
    the max-power lookback above + a code tracking loop filter.

    Code-phase tracking (only) is now the real, C-backed
    `doppler.source.NCO` -- the first piece of this prototype's planned
    migration from pure Python to doppler's actual extension objects,
    piece by piece, verifying each swap keeps the stress/pipeline
    validations green before moving to the next (replica generation,
    loop filter, ...). `NCO` is a pure 32-bit phase accumulator
    (`native/inc/nco/nco_core.h`): `norm_freq = code_rate / tsamps`
    (cycles/sample, recomputed every epoch as the loop filter's estimate
    of `code_rate` moves) and `steps_u32(tsamps)` advances it across the
    epoch and returns the raw `[0, 2**32)` accumulator ramp, which this
    class converts to a continuous chip position (`phase / 2**32 * sf`)
    -- exactly the float `phase`/`phase_inc`/`% 1.0` bookkeeping this
    replaced, just with the NCO's own persistent fixed-point state
    instead of a hand-rolled float accumulator.

    No carrier (see module docstring), no state machine/acquisition --
    assumes the loop starts already roughly code-phase aligned (as
    `Dll`'s `init_chip` does).

    Parameters
    ----------
    code : ndarray of uint8
        Spreading code, one period, 0/1 chips.
    sps : int
        Samples per chip.
    bn : float
        Code loop noise bandwidth, normalised to the once-per-epoch
        update rate (matches `Dll`'s `bn` convention).
    zeta : float
        Damping factor (0.707 = critically damped).
    spacing : float
        Early/late tap offset, chips.
    windows : int
        Partial correlations per epoch; must evenly divide `sf * sps`
        (matches the design doc's own chunk-sizing constraint).
    init_chip : float
        Seed code phase, chips.
    """

    def __init__(
        self, code, sps, bn, zeta=0.707, spacing=0.5, windows=8,
        init_chip=0.0,
    ):
        self.code = code
        self.sf = len(code)
        self.sps = sps
        self.tsamps = self.sf * sps
        if self.tsamps % windows != 0:
            raise ValueError(
                f"tsamps={self.tsamps} not divisible by windows={windows}"
            )
        self.windows = windows
        self.step_size = self.tsamps // windows
        self.spacing = spacing

        self.code_rate = 1.0
        # NCO pinned at the TRUE nominal rate forever (one code period
        # per epoch, norm_freq never touched again) -- the loop's
        # tracked deviation is supplied every epoch through the control
        # port (steps_u32_ctrl), matching the established doppler idiom
        # for a loop-filter-driven oscillator (lo_step_ctrl /
        # lo_steps_ctrl, used e.g. by carrier_nda_core.h), not by
        # overwriting the NCO's own configured rate.
        self._nco = NCO(norm_freq=1.0 / self.tsamps, nmax=0)
        self._nco.phase = int(
            round(((init_chip / self.sf) % 1.0) * 4294967296.0)
        ) & 0xFFFFFFFF
        # Pre-allocated once, reused every epoch via steps_u32_ctrl's
        # `out=` path -- no per-call buffer allocation in the hot loop.
        # `out=` must be sized to the NCO's own steps_u32_ctrl_max_out()
        # (the max any call could ever need), not just this epoch's
        # tsamps; the returned view is still correctly sliced to tsamps.
        self._ctrl_buf = np.full(
            self.tsamps, 0.0, dtype=np.float32
        )
        self._phase_buf = np.empty(
            max(self._nco.steps_u32_ctrl_max_out(), self.tsamps),
            dtype=np.uint32,
        )
        self.lf = LoopFilter(bn, zeta)
        self.last_error = 0.0

        self._last_buff = None
        self._last_early = None
        self._last_late = None
        self._last_backward_sums = None

    def run(self, x, log=None):
        """Process `x` (length a multiple of `sf*sps`) one epoch at a
        time. Returns the flattened chunk-rate output stream (`windows`
        samples per epoch, natural time order)."""
        n_epochs = len(x) // self.tsamps
        out = np.empty(n_epochs * self.windows, dtype=np.complex128)
        for k in range(n_epochs):
            seg = x[k * self.tsamps:(k + 1) * self.tsamps]

            # NCO control port: phase_inc stays at the pinned nominal
            # rate; `ctrl` is this epoch's tracked deviation
            # (code_rate - 1)/tsamps, constant across the epoch since
            # the loop filter only updates once per epoch, added on top
            # for these tsamps steps without ever touching norm_freq.
            # The raw u32 accumulator ramp converts to a continuous chip
            # position via phase / 2**32 * sf. Both buffers are the ones
            # allocated once in __init__ -- fill in place, write through
            # `out=` -- no allocation in this hot per-epoch loop.
            self._ctrl_buf.fill((self.code_rate - 1.0) / self.tsamps)
            phase_u32 = self._nco.steps_u32_ctrl(
                self._ctrl_buf, out=self._phase_buf
            )
            cp = (phase_u32.astype(np.float64) / 4294967296.0) * self.sf
            ce = (cp + self.spacing) % self.sf
            cl = (cp - self.spacing) % self.sf
            punctual = replica2x(self.code, cp)
            early = replica2x(self.code, ce)
            late = replica2x(self.code, cl)

            output = seg * punctual
            (
                power, window, backward_sums, integrate_and_dump,
                window_index,
            ) = find_max_power(
                output, self.windows, self.step_size,
                self._last_backward_sums,
            )
            signal_plus_noise_power = power

            buff_window = get_window(
                seg,
                self._last_buff if self._last_buff is not None else seg,
                window_index,
            )
            early_window = get_window(
                early,
                self._last_early if self._last_early is not None else early,
                window_index,
            )
            late_window = get_window(
                late,
                self._last_late if self._last_late is not None else late,
                window_index,
            )
            self._last_buff = seg
            self._last_early = early
            self._last_late = late
            self._last_backward_sums = backward_sums

            early_power = np.abs((buff_window * early_window).mean()) ** 2
            late_power = np.abs((buff_window * late_window).mean()) ** 2
            e = 0.5 * (early_power - late_power) / (
                signal_plus_noise_power + 1e-12
            )
            self.last_error = e
            lf_out = self.lf.step(e)
            # the working implementation's exact scaling: the loop
            # filter's own update rate is once/epoch, so its output is a
            # "chips per epoch" quantity; dividing by tsamps converts
            # that into the per-sample rate the NCO needs (matches its
            # despreader's `_loop_out_scaling`).
            self.code_rate = 1.0 + lf_out / self.tsamps

            out[k * self.windows:(k + 1) * self.windows] = integrate_and_dump

            if log is not None and k % log == 0:
                print(
                    f"  epoch {k:5d}  window={window}  power={power:.4f}  "
                    f"e={e:+.5f}  code_rate={self.code_rate:.7f}"
                )

        return out
