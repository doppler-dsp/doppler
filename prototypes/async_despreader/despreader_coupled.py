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
Doppler RATE up to +/-500 Hz/s -- corrected from an earlier "+/-5 kHz/s"
typo, 10x too high; see `SPEC.md`'s own corrected figure). This module
is the first attempt at
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
Doppler RATE (a frequency ramp) -- and it's worse than "a bounded
steady-state lag" (the naive type-2-loop expectation: zero steady-state
error for a frequency STEP, a bounded lag for a frequency RAMP). A
later rate-sweep at this module's own `bn=bn_car=0.01` (`SPEC.md`'s
derived floor bound) found a hard CLIFF, not a graceful lag: tracking
stays essentially exact up to ~117 Hz/s and is completely lost --
`car_norm_freq` pinned near its seed while the true frequency runs
away unboundedly -- above ~120 Hz/s (confirmed by an 8000-epoch
time-series trace, not just a final snapshot). `SPEC.md`'s own
corrected worst case (+/-500 Hz/s, was mistyped "+/-5 kHz/s") is
already ~4x past that cliff, so some form of rate-aiding is
structurally required, not optional.

**The real fix (Phase 1f): `bn_fll_car`, not a hand-rolled FFT
re-estimate.** The FIRST FLL-assist attempt (Phase 1d/1e, described
above) invented an entirely separate, from-scratch mechanism: a
periodic batch `freq_refine.estimate_residual_freq` (squaring+FFT)
folded into a second "coarse" deviation register. Investigating why
*that* mechanism was unsafe near the noise floor (gross wrong-peak
corrections, -7800 Hz error at 3dB/500Hz-per-s) led to the question
that mattered -- "where is this Costas?" -- and the discovery that
`costas_core.h`'s own `bn_fll` is EXACTLY this problem, already
solved: a decision-directed cross-product frequency discriminator
(`costas_update`'s `k_fll * freq_err` term, folded straight into the
PI loop's own integrator) with a far wider linear range than the
phase discriminator alone, validated safe at the identical stress
scenario (-25 Hz error at 3dB, vs -7800 Hz for the hand-rolled
mechanism; see `test_real_costas_fll.py` in this session's
investigation, referenced from `FINISHING_PLAN.md`). This module now
mirrors `costas_update` directly: `bn_fll_car` (default 0.0, disabled
-- same convention as `Costas`'s own `bn_fll`) derives `k_fll = 4 *
bn_fll_car`; every epoch, a cross-product between this epoch's and
the previous epoch's despread prompt (both data-wiped by their own
sign, so a BPSK bit flip between epochs doesn't corrupt the product)
nudges `car_lf.integ` directly, ahead of the loop filter's own
`step(e_car)` call -- plus the proportional phase-kick
(`nco.phase += nco_norm_to_inc(kp*e_car/2pi)`) `costas_update` also
applies, previously missing here entirely. There is only ONE tracked
carrier deviation now (`_car_rate_dev`), not two -- the old
coarse/fine split is retired along with the FFT mechanism it existed
to support. `bn_car` and `bn_fll_car` are still two independent knobs
on two different rate regimes, exactly like `Costas`'s own `bn`/
`bn_fll`: `bn_car` sizes how fast the PLL closes a STEP-like residual;
`bn_fll_car` sizes how fast the FLL cross-product term pulls in a
RAMP-induced (or just large-initial) residual the PLL alone cannot
close. `freq_refine.estimate_residual_freq` remains the right tool
for `acq_handoff.py`'s one-shot post-acquisition frequency bridge
(Phase 1c) -- it's `despreader_coupled.py`'s own duplicate
CONTINUOUS-TRACKING use of it, specifically, that's retired here.
"""
from __future__ import annotations

import numpy as np

from doppler.interp import InterpolatedTable
from doppler.source import LO, NCO
from doppler.track import LoopFilter

COSTAS_EPS = 1e-12


def _nco_norm_to_inc(cycles):
    """Cycles -> uint32 phase-delta, matching `nco_core.h`'s
    `nco_norm_to_inc` (floor-normalize then round -- a bare truncating
    cast is UB on a negative value; see that primitive's own doc).
    Not Python-exposed (it's a header-inline C composition primitive),
    so mirrored here rather than adding new bindings for a prototype --
    this file already used the identical formula inline once (the
    `init_chip` phase seed below), now factored so it isn't a second
    private copy."""
    frac = cycles - np.floor(cycles)
    return int(round(frac * 4294967296.0)) & 0xFFFFFFFF


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
    bn_fll_car : float
        FLL-assist bandwidth, same convention as `Costas`'s own
        `bn_fll` -- derives `k_fll = 4 * bn_fll_car`, the gain on the
        per-epoch cross-product frequency discriminator folded into
        `car_lf.integ` ahead of the loop filter's own PLL update.
        Default 0.0 (disabled -- pure PLL, behavior byte-identical to
        before this feature existed). See the module docstring's
        "Phase 1f" section for why this replaced an earlier
        from-scratch periodic-FFT mechanism. Ignored when
        `car_update_windows=True` (FLL-assist is not engaged in that
        mode -- see below).
    car_update_windows : bool
        Default False -- byte-identical to every prior behavior of
        this class (once-per-EPOCH carrier discriminator/loop-filter
        update on the coherent full-epoch prompt, `output.mean()`).
        If True, the carrier loop instead updates once per WINDOW,
        directly off each window's own `integrate_and_dump[i]` partial
        prompt (already computed by `find_max_power` for the code
        loop) -- no data-wiping/combination step needed, since each
        window gets its own real-time discriminator update instead of
        being averaged into one epoch-wide figure first. This fixes
        the once-per-epoch discriminator's own Nyquist margin (the
        legacy_commz reference's nested two-rate loop always updates
        at this granularity; see `FINISHING_PLAN.md`/project memory,
        CHECKPOINT 16). The carrier NCO/LO's ctrl port is driven by a
        `windows`-length per-window deviation array (applied via
        `np.repeat` across each window's `step_size` raw samples),
        preserving the existing one-epoch-delay convention at window
        granularity: the deviation computed for window `i` this epoch
        is applied to window `i`'s samples on the NEXT epoch.

        **`bn_car` is passed straight through UNCHANGED -- do NOT
        divide it by `windows` or any per-epoch/per-symbol ratio.**
        `LoopFilter(bn, zeta, t=1.0)` is already per-UPDATE normalized
        (`t=1.0` fixed regardless of how many real samples an update
        spans); `kp` scales ~linearly with `bn` but `ki` scales
        ~QUADRATICALLY (standard 2nd-order PI loop filter, confirmed
        directly against `doppler.track.LoopFilter`: `ki/bn**2` is
        ~constant across an 8x `bn` sweep). Dividing `bn_car` by
        `windows` (an earlier version of this docstring's own advice,
        "typically bn_car_symbol_normalized / windows_per_symbol" --
        WRONG, see CHECKPOINT 25 in `FINISHING_PLAN.md`) under-scales
        the integrator -- the mechanism that gives a type-2 loop its
        ramp/rate-tracking capability -- by an EXTRA factor of
        `windows` on top of the `windows`-times-more-frequent updates,
        crippling real-Doppler-RATE tracking specifically (confirmed:
        divided, BER~0.5 on a real 500Hz/s case; undivided, BER=0.0000,
        no FLL-assist needed at all). Running the identical `(bn_car,
        zeta)` at a higher update rate already widens the real-Hz
        bandwidth correctly on its own -- that's the whole point of
        this mode, no manual rescaling required. FLL-assist
        (`bn_fll_car`) is not engaged in this mode (and isn't needed --
        the `~/legacy-commz` reference also tracks a real Doppler rate
        with a plain Costas PLL, no FLL cross-product term at all).
    """

    def __init__(
        self, code, sps, bn, zeta=0.707, spacing=0.5, windows=8,
        init_chip=0.0, bn_car=None, init_car_norm_freq=0.0,
        aid_code=True, sample_rate_hz=4.092e6, carrier_freq_hz=2.2e9,
        freeze_carrier=False, bn_fll_car=0.0, car_update_windows=False,
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
        self._nco.phase = _nco_norm_to_inc(init_chip / self.sf)
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
        self.car_update_windows = car_update_windows
        # Per-window pure-deviation control array (natural window
        # order, same indexing as find_max_power's integrate_and_dump)
        # -- one-epoch-delay convention at window granularity: index i
        # here was computed FROM window i's prompt last epoch, and gets
        # applied TO window i's raw samples this epoch via np.repeat.
        self._car_ctrl_windows = np.zeros(self.windows, dtype=np.float64)
        self.car_norm_freq = init_car_norm_freq  # public display only
        # If True, the carrier LO derotates every epoch at its pinned
        # seed rate only -- the discriminator still computes e_car (for
        # inspection) but never updates car_lf/_car_rate_dev. Same
        # isolate-the-mechanism purpose as aid_code=False: lets a
        # frequency-refinement stage (Phase 1c) collect a clean
        # residual-tone signal from `out` before the closed loop starts
        # adapting, instead of chasing a moving target.
        self.freeze_carrier = freeze_carrier

        # ── FLL-assist: costas_update's own cross-product frequency
        # discriminator, folded directly into car_lf.integ -- see
        # module docstring's "Phase 1f" section. k_fll=0 (bn_fll_car=0)
        # is a pure PLL, byte-identical to before this feature existed.
        self._k_fll = 4.0 * bn_fll_car
        self._car_prev_prompt = None

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
            # (must happen here, pre-despread -- a downstream-only
            # correction lets the signal walk out of the correlator's
            # bandwidth over minutes of operation; see module
            # docstring.) The tracked deviation flows through the ctrl
            # port -- the LO's own pinned norm_freq is never touched.
            if self.car_update_windows:
                self._car_ctrl_buf[:] = np.repeat(
                    self._car_ctrl_windows, self.step_size
                )
            else:
                self._car_ctrl_buf.fill(self._car_rate_dev)
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

            if self.car_update_windows:
                # ── Per-WINDOW carrier discriminator: costas_update's
                # sign(Re)*Im/|.| form, run once per window directly on
                # `integrate_and_dump[i]` (already computed by
                # find_max_power for the code loop) -- no lookback/
                # combination needed, each window is its own real-time
                # update. FLL-assist is not engaged in this mode (see
                # class docstring); `bn_car` is interpreted at this
                # per-window rate, so the radians->cycles/sample
                # conversion below uses step_size, not tsamps.
                for i in range(self.windows):
                    prompt_i = integrate_and_dump[i]
                    re_p, im_p = prompt_i.real, prompt_i.imag
                    a_p = abs(prompt_i) + COSTAS_EPS
                    e_car = (im_p if re_p >= 0.0 else -im_p) / a_p
                    if not self.freeze_carrier:
                        car_lf_out = self.car_lf.step(e_car)
                        self._car_ctrl_windows[i] = (
                            car_lf_out / (self.step_size * 2.0 * np.pi)
                        )
                        kick = self.car_lf.kp * e_car / (2.0 * np.pi)
                        self._car_lo.phase = (
                            self._car_lo.phase + _nco_norm_to_inc(kick)
                        ) & 0xFFFFFFFF
                self.car_last_error = e_car
                self._car_rate_dev = self._car_ctrl_windows[-1]
                self.car_norm_freq = (
                    self._init_car_norm_freq + self._car_rate_dev
                )
            else:
                # ── Carrier discriminator: Costas' own sign(Re)*Im/|.|
                # form, on this epoch's COHERENT full-epoch prompt (no
                # lookback needed for carrier tracking -- see module
                # docstring). TEMPORARILY REVERTED (task #100's own
                # data-wiping fix, backed out for an ablation
                # comparison -- see FINISHING_PLAN.md/project memory
                # for the measured before/after; likely to be
                # re-applied).
                prompt = output.mean()
                re_p, im_p = prompt.real, prompt.imag
                a_p = abs(prompt) + COSTAS_EPS
                e_car = (im_p if re_p >= 0.0 else -im_p) / a_p
                self.car_last_error = e_car
                if not self.freeze_carrier:
                    # FLL-assist: costas_update's own cross-product
                    # frequency discriminator, folded directly into
                    # car_lf.integ AHEAD of this epoch's step(e_car) --
                    # both prompts data-wiped by their own Re sign so a
                    # BPSK bit flip between epochs doesn't corrupt the
                    # cross product. Far wider linear range than the
                    # phase discriminator alone; see module docstring.
                    if (
                        self._k_fll > 0.0
                        and self._car_prev_prompt is not None
                    ):
                        rpr = self._car_prev_prompt.real
                        ipr = self._car_prev_prompt.imag
                        sc = 1.0 if re_p >= 0.0 else -1.0
                        sp = 1.0 if rpr >= 0.0 else -1.0
                        ic, qc = re_p * sc, im_p * sc
                        ip, qp = rpr * sp, ipr * sp
                        cross = ip * qc - qp * ic
                        a_pr = abs(self._car_prev_prompt) + COSTAS_EPS
                        freq_err = cross / (a_p * a_pr)
                        self.car_lf.integ += self._k_fll * freq_err
                    self._car_prev_prompt = prompt

                    car_lf_out = self.car_lf.step(e_car)  # radians/epoch
                    # radians/epoch -> cycles/sample: /(2*pi) then
                    # *inv_tsamps. Pure deviation -- the LO's own
                    # pinned norm_freq (the nominal/seed) is never
                    # touched; only the ctrl port sees this.
                    # car_norm_freq (below) adds the two back together
                    # for display only, mirroring code_rate = 1.0 +
                    # rate_dev.
                    self._car_rate_dev = (
                        car_lf_out * self._inv_tsamps / (2.0 * np.pi)
                    )
                    # Proportional phase kick (costas_update's other
                    # term, previously missing here): kp*e_car radians
                    # -> cycles -> uint32 phase delta, applied directly
                    # to the carrier LO's own accumulator -- speeds
                    # lock the pure frequency-integral path alone
                    # can't.
                    kick = self.car_lf.kp * e_car / (2.0 * np.pi)
                    self._car_lo.phase = (
                        self._car_lo.phase + _nco_norm_to_inc(kick)
                    ) & 0xFFFFFFFF

                self.car_norm_freq = (
                    self._init_car_norm_freq + self._car_rate_dev
                )

            if self.aid_code:
                # Physical coupling: the SAME v/c that shifts the
                # carrier shifts the code clock by the identical
                # FRACTIONAL amount -- scale by sample_rate_hz/
                # carrier_freq_hz to convert into the code loop's own
                # dimensionless rate-ratio deviation. Must aid from the
                # FULL current carrier estimate (fixed nominal seed +
                # whatever the closed loop has since tracked), not just
                # the tracked deviation alone -- a nonzero
                # `init_car_norm_freq` (e.g. an Acquisition/PSDMF seed)
                # implies its own physical code-rate offset regardless
                # of whether the closed carrier loop has corrected
                # anything yet. Mirrors legacy_commz's own
                # `_loop_doppler_aid` (from the fixed nominal, set once
                # at code lock) + `_loop_doppler_aid_carrier_loop_
                # contribution` (from the closed loop's own incremental
                # stress), added together -- found missing the nominal
                # term this session while comparing against that
                # reference (`~/legacy-commz`). No-op for every existing
                # caller in this folder that pairs `aid_code=True` with
                # `init_car_norm_freq=0.0` (doppler_rate_test.py/
                # doppler_rate_floor_test.py); changes acq_handoff.py's
                # own behavior (nonzero seed there) -- re-verified it
                # still locks after this fix.
                aid_dev = (
                    self._init_car_norm_freq + self._car_rate_dev
                ) * self._aid_scale
                self._rate_dev = code_disc_dev + aid_dev
            else:
                self._rate_dev = code_disc_dev
            self.code_rate = 1.0 + self._rate_dev

            out[k * self.windows:(k + 1) * self.windows] = integrate_and_dump

            if log is not None and k % log == 0:
                print(
                    f"  epoch {k:5d}  window={window}  power={power:.4f}  "
                    f"e={e:+.5f}  code_rate={self.code_rate:.7f}  "
                    f"e_car={e_car:+.5f}  car_norm_freq={self.car_norm_freq:.3e}"
                )

        return out
