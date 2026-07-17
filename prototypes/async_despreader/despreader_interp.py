"""Same design as `despreader_lf.py` (NCO + real `doppler.track.
LoopFilter` already swapped in), with replica generation now ALSO
swapped to a real, C-backed object: `doppler.interp.InterpolatedTable`.

This is the third step of migrating the validated pure-Python prototype
to doppler's actual extension objects -- see `compare_interp.py`, which
compares this module against `despreader_lf.py` (the previous step) to
isolate just this one swap.

Unlike NCO/LoopFilter, there was no existing standalone "replica"
primitive to swap into -- `dll_replica()` (`native/inc/dll/dll_core.h`)
is a private helper baked directly into `Dll`, not exposed to Python.
`InterpolatedTable` is a genuinely NEW, general-purpose doppler object
(`native/inc/interp_table/interp_table_core.h`, ported faithfully from
a working implementation's own `InterpolatedTable` class: periodic
mod-length wraparound indexing + floor/nearest/linear interpolation
over an arbitrary complex table) -- not DSSS-specific at all, reusable
for any periodic-table lookup. The despreader builds a 2x-oversampled
±1-chip-sign table once at construction (`table2x[2c] == table2x[2c+1]
== csign[c]`, matching `replica2x`'s "hold" convention exactly) and
queries it with the SAME `c*2 - 0.5` position transform `replica2x`
used -- `InterpolatedTable`'s own wraparound handles the rest
internally (no need to pre-wrap the position first: wrapping by any
whole multiple of the table length before taking the fractional part
is a no-op, so passing the raw, unwrapped position is exactly
equivalent to `replica2x`'s manual `% (2*sf)` pre-wrap).

Adding this object surfaced the SAME jm codegen leak bug found and
fixed for `NCO`/`LO` (cached buffer + gh-437 weakref-gated retire under
the `x = obj.method(...)` loop pattern) -- fixed the same way
(`InterpolatedTableObj_execute` in `native/src/interp/
interp_ext_interp_table.c` allocates fresh per call by default), with
the same permanent regression tests
(`test_execute_no_aliasing_across_calls`, `test_execute_no_leak_in_
tight_loop` in `src/doppler/interp/tests/test_interp_table.py`).
"""
from __future__ import annotations

import numpy as np

from doppler.interp import InterpolatedTable
from doppler.source import NCO
from doppler.track import LoopFilter


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


class SimpleAsyncDespreader:
    """Minimal, faithful port: NCO-driven 2x-oversampled code replica +
    the max-power lookback above + a code tracking loop filter.

    Code-phase tracking (`doppler.source.NCO`) and the code loop filter
    (`doppler.track.LoopFilter`) are now both the real, C-backed
    objects -- the first two pieces of this prototype's planned
    migration from pure Python to doppler's actual extension objects,
    piece by piece, verifying each swap keeps the stress/pipeline
    validations green before moving to the next (replica generation
    next). `NCO` is a pure 32-bit phase accumulator
    (`native/inc/nco/nco_core.h`): `norm_freq = code_rate / tsamps`
    is pinned at the TRUE nominal rate (one code period per epoch) at
    construction and never touched again; the loop's tracked deviation
    is supplied every epoch through the control port
    (`steps_u32_ctrl`), matching the established doppler idiom for a
    loop-filter-driven oscillator (`lo_step_ctrl`/`lo_steps_ctrl`, used
    e.g. by `carrier_nda_core.h`) rather than overwriting the NCO's own
    configured rate. The raw `[0, 2**32)` accumulator ramp converts to a
    continuous chip position via `phase / 2**32 * sf`. `LoopFilter` (module
    docstring above) is a genuinely different filter algorithm, not
    just lower precision -- constructed with `t=1.0` to match its gain
    formula to this class's own once-per-epoch update rate, and its
    full `step()` output is scaled the validated way (`code_rate = 1.0
    + lf_out / tsamps`), not the scheme the currently-committed C
    `dll_update()` uses.

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
        self._nco = NCO(norm_freq=1.0 / self.tsamps, nmax=0)
        self._nco.phase = int(
            round(((init_chip / self.sf) % 1.0) * 4294967296.0)
        ) & 0xFFFFFFFF
        # Pre-allocated once, reused every epoch via steps_u32_ctrl's
        # `out=` path -- no per-call buffer allocation in the hot loop.
        # `out=` must be sized to the NCO's own steps_u32_ctrl_max_out()
        # (the max any call could ever need), not just this epoch's
        # tsamps; the returned view is still correctly sliced to tsamps.
        self._ctrl_buf = np.full(self.tsamps, 0.0, dtype=np.float32)
        self._phase_buf = np.empty(
            max(self._nco.steps_u32_ctrl_max_out(), self.tsamps),
            dtype=np.uint32,
        )
        # 2x-oversampled, dwell-center-correct code replica table
        # (matches `dll_replica`/`replica2x`'s "hold" convention:
        # table2x[2c] == table2x[2c+1] == csign[c]), queried via the same
        # `c*2 - 0.5` position transform -- InterpolatedTable's own
        # wraparound makes pre-wrapping the position unnecessary.
        csign = np.where(code & 1, -1.0, 1.0)
        table2x = np.repeat(csign, 2).astype(np.complex128)
        self._replica = InterpolatedTable(table2x, method="linear")
        # Pre-allocated once, reused every epoch via execute()'s `out=`
        # path -- unlike NCO/LO, InterpolatedTable's out= only needs to
        # be sized to the call's own input length (execute_max_out() is
        # always 0: output length is deterministically tied to input
        # length), no separate larger fixed cap. Only `punctual` gets
        # this treatment -- see the run() comment on why early/late
        # can't share a reused buffer across epochs.
        self._punct_buf = np.empty(self.tsamps, dtype=np.complex128)
        self.lf = LoopFilter(bn=bn, zeta=zeta, t=1.0)
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
            # Both buffers are the ones allocated once in __init__ --
            # fill in place, write through `out=` -- no allocation in
            # this hot per-epoch loop.
            self._ctrl_buf.fill((self.code_rate - 1.0) / self.tsamps)
            phase_u32 = self._nco.steps_u32_ctrl(
                self._ctrl_buf, out=self._phase_buf
            )
            cp = (phase_u32.astype(np.float64) / 4294967296.0) * self.sf
            ce = (cp + self.spacing) % self.sf
            cl = (cp - self.spacing) % self.sf
            # No pre-wrap needed (see __init__'s comment); execute()'s
            # own wraparound handles the raw, unwrapped position.
            # `punctual` is only ever read within this same epoch (used
            # immediately below, never stashed), so its buffer is safe
            # to reuse via out=. `early`/`late` are DIFFERENT: they get
            # stashed as self._last_early/self._last_late for the NEXT
            # epoch's get_window() to read -- reusing a buffer for those
            # would let this epoch's write silently corrupt the "last
            # epoch" data get_window() depends on, so they stay on the
            # safe default (a fresh, independent array every call).
            punctual = self._replica.execute(
                cp * 2.0 - 0.5, out=self._punct_buf
            )
            early = self._replica.execute(ce * 2.0 - 0.5)
            late = self._replica.execute(cl * 2.0 - 0.5)

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
