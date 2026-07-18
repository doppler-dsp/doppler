"""Phase 1 of the coherent, carrier-aided DSSS receiver roadmap
(`~/.claude/plans/jiggly-munching-newell.md`): a carrier NCO derotates
the RAW chip-rate samples BEFORE despreading, instead of `Costas`/
`CarrierNda` running downstream on already-despread symbols
(`despreader_interp.py`'s architecture, and today's shipped `Dll`).

Why: the shipped, documented architecture ("why the carrier belongs
downstream", `docs/design/async-symbol-despreader.md`) only ever
analyzed a FIXED residual carrier's one-epoch I&D loss. It never
analyzed carrier RATE OF CHANGE -- the actual failure mode for the
S-band LEO use case this receiver targets (confirmed numbers already
in `docs/design/dsss-acquisition.md` Sec8: Doppler up to +/-100 kHz,
Doppler RATE up to +/-5 kHz/s). This module is the first attempt at
the alternative: carrier-aided code tracking, the standard GNSS/DSSS
technique -- carrier Doppler and code-rate Doppler are physically
coupled through `sample_rate/carrier_freq` (both scale with the same
v/c), so the tracked carrier rate directly aids the code NCO's rate on
top of the code loop's own discriminator, and a chip-rate carrier NCO
gets a much wider/faster tracking response than a downstream loop
waiting for full despread gain to accumulate first.

Architecture, once per epoch:
  1. Generate this epoch's carrier phasor from the carrier LO's own
     control port (nominal pinned, tracked deviation added via ctrl --
     same pattern as the code NCO already uses).
  2. Derotate: `seg_wiped = seg * conj(carrier_phasor)`.
  3. Feed `seg_wiped` through EXACTLY `despreader_interp.py`'s existing
     despread pipeline (code NCO, InterpolatedTable replica, max-power
     lookback, E/P/L accumulation, code discriminator/loop) --
     unchanged, reused verbatim, not reimplemented.
  4. Carrier discriminator: Costas' own sign(Re)*Im/|.| form
     (`costas_core.h`'s `costas_update`), on this epoch's COHERENT
     full-epoch prompt (`output.mean()` -- carrier tracking doesn't
     need the code loop's async-data lookback machinery; a lone
     mid-epoch data flip just costs one noisy epoch, which the loop
     filter already smooths over, same as it would for the code loop's
     own "natural window" case).
  5. Carrier loop filter -> a PURE rate deviation (radians/epoch ->
     cycles/sample, no "0 Hz nominal" mixed in) -> the carrier LO's
     ctrl port for the NEXT epoch.
  6. Optional code-rate AIDING: the tracked carrier-rate deviation,
     scaled by `sample_rate_hz / carrier_freq_hz` (the physical
     v/c-coupling ratio), is added -- as a second PURE deviation, no
     re-derivation of "nominal" -- directly into the code loop's own
     `_rate_dev` before it drives the code NCO's ctrl port. One-way
     (carrier -> code) only, per this initial pass; two-way aiding is
     an open question for a later round.

Deliberately NOT yet done here (see the plan's own Phase 1 open
questions): carrier-loop-bandwidth-vs-code-loop-bandwidth ordering is a
bare guess (`bn_car` wider than `bn` by 10x, unvalidated); aiding is
toggleable (`aid_code=`) so the carrier loop and the aiding path can be
validated independently before validating them together.

**FLL-assist (added, Phase 1d/1e)**: Costas alone cannot track a
Doppler RATE (a frequency ramp) without a bounded steady-state lag --
it's a type-2 phase loop, so it has zero steady-state error for a
frequency STEP but not for a frequency RAMP (`doppler_rate_test.py`
measured this directly: at the LEO worst-case rate, +-5 kHz/s, a
static one-shot version of the same estimator used here was off by
>9 kHz; a periodically-re-seeded version tracked to ~60 Hz). Set
`fll_block_epochs` to enable a SECOND, slower carrier-tracking axis:
every that many epochs, the despread output accumulated over the block
is run through `freq_refine.estimate_residual_freq` (the exact same
primitive Phase 1c already validated -- no new discriminator), and the
result becomes a correction to a separate COARSE deviation register,
left entirely apart from Costas's own fast, continuous FINE deviation.
The two are pure deviations, combined only where they're actually used
(the LO's ctrl port, the display `car_norm_freq`, and the code-aiding
scale) -- never re-mixed into each other's state, same discipline as
the rest of this project's control-path architecture. `bn_car` and
`fll_block_epochs` are thus two independent knobs on two different
rate regimes: `bn_car` sizes how fast Costas closes a STEP-like
residual; `fll_block_epochs` sizes how often the coarse loop corrects
the RAMP-induced lag Costas structurally cannot close on its own.
"""
from __future__ import annotations

import numpy as np

from doppler.interp import InterpolatedTable
from doppler.source import LO, NCO
from doppler.track import LoopFilter
from freq_refine import estimate_residual_freq

COSTAS_EPS = 1e-12


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
    """Identical to despreader_interp.py's own -- see there for the
    full explanation. Duplicated here (not imported) only because this
    module is a standalone prototype file, same as despreader_lf.py /
    despreader_interp.py each were relative to their predecessor."""
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


class CoupledAsyncDespreader:
    """Carrier-aided async DSSS despreader: chip-rate carrier NCO
    derotation + the existing (unmodified) code-tracking pipeline.

    Parameters
    ----------
    code, sps, bn, zeta, spacing, windows, init_chip
        Same as `despreader_interp.SimpleAsyncDespreader` -- the code
        loop's own parameters, unchanged.
    bn_car : float
        Carrier loop noise bandwidth, same once-per-epoch-update
        convention as `bn`. Default 10x `bn` -- carrier tracking must
        respond faster than code tracking (the whole point of aiding
        the SLOWER loop from the FASTER one), but the exact ratio is
        unvalidated; see the module docstring.
    init_car_norm_freq : float
        Seed carrier frequency, cycles/sample (e.g. from Acquisition's
        Doppler estimate / sample_rate_hz). Default 0.0.
    aid_code : bool
        If True, the tracked carrier-rate deviation aids the code
        loop's own rate_dev via the `sample_rate_hz/carrier_freq_hz`
        coupling. If False, the carrier loop still runs (and its own
        tracking can be validated) but never touches the code loop --
        useful for isolating the two mechanisms while validating.
    sample_rate_hz, carrier_freq_hz : float
        Only used when `aid_code=True` -- the physical v/c-coupling
        ratio. Defaults are a representative S-band LEO link (sample
        rate matching `sf*sps` at `sf=1023`/2.046 Mcps chip rate/2
        samples-per-chip -> 4.092 Msps; carrier_freq_hz=2.2e9, a
        typical S-band downlink) -- override for any other setup.
    fll_block_epochs : int or None
        If set, enables FLL-assist: every this many epochs, run
        `freq_refine.estimate_residual_freq` over the accumulated
        despread output and fold the result into a COARSE carrier
        deviation register, separate from Costas's own continuous fine
        deviation -- see the module docstring. `None` (default)
        disables it entirely; behavior is then byte-identical to
        before this feature existed.
    fll_n_fft, fll_zero_pad : int
        Passed straight through to `estimate_residual_freq` -- Phase
        1c's own validated defaults (64, 4).
    """

    def __init__(
        self, code, sps, bn, zeta=0.707, spacing=0.5, windows=8,
        init_chip=0.0, bn_car=None, init_car_norm_freq=0.0,
        aid_code=True, sample_rate_hz=4.092e6, carrier_freq_hz=2.2e9,
        freeze_carrier=False, fll_block_epochs=None, fll_n_fft=64,
        fll_zero_pad=4,
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
        self.aid_code = aid_code
        self._aid_scale = sample_rate_hz / carrier_freq_hz
        self._sample_rate_hz = sample_rate_hz

        self._inv_tsamps = 1.0 / self.tsamps
        self.code_rate = 1.0
        self._rate_dev = 0.0
        self._nco = NCO(norm_freq=1.0 / self.tsamps, nmax=0)
        self._nco.phase = int(
            round(((init_chip / self.sf) % 1.0) * 4294967296.0)
        ) & 0xFFFFFFFF
        self._ctrl_buf = np.full(self.tsamps, 0.0, dtype=np.float32)
        self._phase_buf = np.empty(
            max(self._nco.steps_u32_ctrl_max_out(), self.tsamps),
            dtype=np.uint32,
        )
        csign = np.where(code & 1, -1.0, 1.0)
        table2x = np.repeat(csign, 2).astype(np.complex128)
        self._replica = InterpolatedTable(table2x, method="linear")
        self._punct_buf = np.empty(self.tsamps, dtype=np.complex128)
        self.lf = LoopFilter(bn=bn, zeta=zeta, t=1.0)
        self.last_error = 0.0

        # ── Carrier NCO/loop -- own state, same control-port pattern.
        # nominal pinned at construction, only a pure deviation ever
        # flows through the ctrl port. LO (not NCO) is used here since
        # the derotation needs the actual CF32 phasor, not a phase
        # ramp -- LO's own LUT does that conversion.
        if bn_car is None:
            bn_car = 10.0 * bn
        self._init_car_norm_freq = init_car_norm_freq
        self._car_lo = LO(norm_freq=init_car_norm_freq)
        self._car_ctrl_buf = np.zeros(self.tsamps, dtype=np.float32)
        self._car_rate_dev = 0.0  # Costas's own FINE pure deviation
        self.car_lf = LoopFilter(bn=bn_car, zeta=zeta, t=1.0)
        self.car_last_error = 0.0
        self.car_norm_freq = init_car_norm_freq  # public display only
        # If True, the carrier LO derotates every epoch at its pinned
        # seed rate only -- the discriminator still computes e_car (for
        # inspection) but never updates car_lf/_car_rate_dev. Same
        # isolate-the-mechanism purpose as aid_code=False: lets a
        # frequency-refinement stage (Phase 1c) collect a clean
        # residual-tone signal from `out` before the closed loop starts
        # adapting, instead of chasing a moving target.
        self.freeze_carrier = freeze_carrier

        # ── FLL-assist: a SECOND, independent pure deviation, updated
        # periodically (not every epoch) -- see module docstring for
        # why this doesn't double-count against Costas's own fine
        # deviation above.
        self._car_coarse_dev = 0.0
        self._fll_block_epochs = fll_block_epochs
        self._fll_n_fft = fll_n_fft
        self._fll_zero_pad = fll_zero_pad
        self.fll_corrections = 0
        if fll_block_epochs is not None:
            self._fll_buf = np.empty(
                fll_block_epochs * windows, dtype=np.complex128
            )
            self._fll_pos = 0
            self._fll_epoch_count = 0
        else:
            self._fll_buf = None

        self._last_buff = None
        self._last_early = None
        self._last_late = None
        self._last_backward_sums = None

    def run(self, x, log=None):
        """Process `x` (length a multiple of `sf*sps`) one epoch at a
        time. Returns the flattened chunk-rate output stream (`windows`
        samples per epoch, natural time order) -- still carrying the
        residual carrier the carrier loop hasn't fully nulled, exactly
        like the downstream-only design's output (a downstream Costas
        trim stage still makes sense; this only handles the BULK
        Doppler and its rate so the code loop never sees it)."""
        n_epochs = len(x) // self.tsamps
        out = np.empty(n_epochs * self.windows, dtype=np.complex128)
        for k in range(n_epochs):
            seg = x[k * self.tsamps:(k + 1) * self.tsamps]

            # ── Carrier: derotate raw samples BEFORE despreading ──
            # Both the coarse (FLL, periodic) and fine (Costas,
            # per-epoch) deviations flow through the SAME ctrl port --
            # the LO's own pinned norm_freq is never touched, only
            # combined here at the point of use.
            self._car_ctrl_buf.fill(self._car_coarse_dev + self._car_rate_dev)
            wiped = self._car_lo.steps_ctrl(self._car_ctrl_buf)
            seg_wiped = seg * np.conj(wiped)

            # ── Code tracking: unchanged pipeline, fed seg_wiped ──
            self._ctrl_buf.fill(self._rate_dev * self._inv_tsamps)
            phase_u32 = self._nco.steps_u32_ctrl(
                self._ctrl_buf, out=self._phase_buf
            )
            cp = (phase_u32.astype(np.float64) / 4294967296.0) * self.sf
            ce = (cp + self.spacing) % self.sf
            cl = (cp - self.spacing) % self.sf
            punctual = self._replica.execute(
                cp * 2.0 - 0.5, out=self._punct_buf
            )
            early = self._replica.execute(ce * 2.0 - 0.5)
            late = self._replica.execute(cl * 2.0 - 0.5)

            output = seg_wiped * punctual
            (
                power, window, backward_sums, integrate_and_dump,
                window_index,
            ) = find_max_power(
                output, self.windows, self.step_size,
                self._last_backward_sums,
            )
            signal_plus_noise_power = power

            buff_window = get_window(
                seg_wiped,
                self._last_buff if self._last_buff is not None
                else seg_wiped,
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
            self._last_buff = seg_wiped
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
            code_disc_dev = lf_out * self._inv_tsamps

            # ── Carrier discriminator: Costas' own sign(Re)*Im/|.| form,
            # on this epoch's COHERENT full-epoch prompt (no lookback
            # needed for carrier tracking -- see module docstring). ──
            prompt = output.mean()
            re_p, im_p = prompt.real, prompt.imag
            a_p = abs(prompt) + COSTAS_EPS
            e_car = (im_p if re_p >= 0.0 else -im_p) / a_p
            self.car_last_error = e_car
            if not self.freeze_carrier:
                car_lf_out = self.car_lf.step(e_car)  # radians/epoch
                # radians/epoch -> cycles/sample: /(2*pi) then
                # *inv_tsamps. Pure deviation -- the LO's own pinned
                # norm_freq (the nominal/seed) is never touched; only
                # the ctrl port sees this. car_norm_freq (below) adds
                # the two back together for display only, mirroring
                # code_rate = 1.0 + rate_dev.
                self._car_rate_dev = (
                    car_lf_out * self._inv_tsamps / (2.0 * np.pi)
                )

            self.car_norm_freq = (
                self._init_car_norm_freq + self._car_coarse_dev
                + self._car_rate_dev
            )

            total_car_dev = self._car_coarse_dev + self._car_rate_dev
            if self.aid_code:
                # Physical coupling: the SAME v/c that shifts the
                # carrier by its TOTAL tracked deviation (coarse FLL +
                # fine Costas, cycles/sample, i.e. fraction of
                # sample_rate_hz) shifts the code clock by the
                # identical FRACTIONAL amount -- scale by
                # sample_rate_hz/carrier_freq_hz to convert into the
                # code loop's own dimensionless rate-ratio deviation,
                # then just ADD it -- a second pure deviation, no
                # re-derivation of "nominal" anywhere.
                aid_dev = total_car_dev * self._aid_scale
                self._rate_dev = code_disc_dev + aid_dev
            else:
                self._rate_dev = code_disc_dev
            self.code_rate = 1.0 + self._rate_dev

            out[k * self.windows:(k + 1) * self.windows] = integrate_and_dump

            # ── FLL-assist: accumulate this epoch's despread output;
            # every fll_block_epochs epochs, re-estimate the residual
            # (Phase 1c's own primitive) and fold it into the COARSE
            # deviation -- corrects the ramp-induced lag Costas's fine
            # loop structurally can't close on its own (see module
            # docstring). Left entirely separate from _car_rate_dev.
            if self._fll_buf is not None:
                self._fll_buf[
                    self._fll_pos:self._fll_pos + self.windows
                ] = integrate_and_dump
                self._fll_pos += self.windows
                self._fll_epoch_count += 1
                if self._fll_epoch_count == self._fll_block_epochs:
                    window_rate_hz = self._sample_rate_hz / self.step_size
                    residual_hz = estimate_residual_freq(
                        self._fll_buf, window_rate_hz,
                        n_fft=self._fll_n_fft,
                        zero_pad=self._fll_zero_pad, interp=True,
                    )
                    self._car_coarse_dev += residual_hz / self._sample_rate_hz
                    self.fll_corrections += 1
                    self._fll_pos = 0
                    self._fll_epoch_count = 0

            if log is not None and k % log == 0:
                print(
                    f"  epoch {k:5d}  window={window}  power={power:.4f}  "
                    f"e={e:+.5f}  code_rate={self.code_rate:.7f}  "
                    f"e_car={e_car:+.5f}  car_norm_freq={self.car_norm_freq:.3e}"
                )

        return out
