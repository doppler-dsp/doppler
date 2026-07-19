"""Phase 1c of the coupled-tracker roadmap
(`~/.claude/plans/jiggly-munching-newell.md`): a frequency-refinement
bridge that closes the gap `pullin_sweep.py` (Phase 1b) measured
between `Acquisition`'s real seed quality (mean |error| ~685 Hz,
worst case 1320+ Hz -- see `project_dsss_acq_async_story.md`'s
Acquisition-isolation section) and the coupled tracker's own bare
pull-in range (locks through ~100 Hz, fails at 200 Hz+).

Replaces legacy-commz's two bespoke `COARSE_FREQUENCY`/
`FINE_FREQUENCY` `FrequencyAcquisition` stages with ONE reused doppler
primitive, per this story's reduced-scope decision -- no new FFT
implementation, no code-phase search (code phase is already resolved
by the time this runs; `acq_core.c`'s 2-D code+Doppler engine has
nothing left to search and is the wrong tool here).

Mechanism (standard, non-data-aided frequency estimation):

1. **Square** the despread, window-rate output stream. Squaring a
   BPSK-modulated tone (`+-1 * exp(j*2*pi*f*n)`) removes the +-1 data
   ambiguity at the cost of doubling the tone's own frequency --
   exactly the squaring-loop technique behind
   `[[reference_squaring_loss_ebno_not_cno]]`, already established
   in this project, applied here for frequency estimation rather
   than a lock-detector SNR formula. Without this step, the data
   modulation aliases the residual tone across the ENTIRE FFT (the
   `doppler_resolution`-engaged-on-`DsssReceiver` investigation, same
   story, found and characterized exactly this failure mode). This
   is also exactly why this bridge's accuracy is Es/N0-sensitive in a
   NONLINEAR way (squaring loss) rather than degrading gracefully like
   a linear discriminator would -- see `characterize_snr.py`, which
   sweeps Es/N0 = 2/5/10/20/50 dB specifically to characterize this.
2. **FFT peak search** over one or more coherent blocks, using
   `doppler.spectral.FFT` (already exists -- reused, not
   reimplemented). Non-coherently accumulates `|FFT|^2` across blocks
   to smooth noise, mirroring `Acquisition`'s own `n_noncoh` knob.
   Optionally **zero-padded** (`zero_pad>1`) before each block's FFT --
   does not add real spectral resolution (still set by the coherent
   block's own time duration) but removes picket-fence/scalloping bias
   by sampling the same continuous spectrum more finely, and optionally
   **parabolic-interpolated** (`interp=True`) around the accumulated
   power spectrum's peak bin for a further sub-bin refinement -- both
   are the standard pair of techniques for exactly this problem, used
   together here per the user's own suggestion rather than picking one.
3. Halve the peak (or interpolated) bin's frequency to undo the
   squaring and recover the residual carrier estimate in Hz, at the
   window-rate stream's own sample rate -- unambiguous as long as
   `2 * search range < window sample rate` (true for this story's
   Doppler numbers; a larger physical range, e.g. the full +-100 kHz
   LEO span, would need a coarser first look or a higher window rate
   -- open question, not solved here, see the module docstring's own
   scope note).

**Zero-padding/interpolation only sharpen a correct peak -- they do
NOT rescue a peak the noise picked at the wrong bin outright.** At low
Es/N0 the failure mode is categorically different (gross error, not
quantization bias); `characterize_snr.py` reports both separately
(mean/RMS error among trials that found the RIGHT peak vs. the
fraction of trials that didn't) rather than blending them into one
number that would hide the transition.

Two follow-on improvements, tried directly against that low-Es/N0
gross-error mode (both suggested by the user; results in
`improve_low_snr.py`):

- **Multi-look non-coherent averaging -- WORKS, the real fix.**
  `estimate_residual_freq` already non-coherently sums `|FFT|^2` over
  every `n_fft`-sample block in `x` -- so "more looks" is just "more
  epochs in the collection window," a caller-side knob (`refine_seed`'s
  `rx_prefix` length), not a new code path. Measured: at Es/N0=2 dB,
  `found_right_peak` goes 0.17-0.25 (300 epochs) -> 0.58-0.83 (900) ->
  0.83-0.92 (2700); by 5 dB it's ~1.0 even at 300 epochs and the
  collection length mainly sharpens RMS error instead (6+ Hz -> ~2 Hz).
- **Matched-filter the known mainlobe shape** (`use_mf=True`,
  `_matched_filter_power`) -- **tried, measured, does NOT help.**
  Cross-correlating the accumulated power spectrum against the known
  Dirichlet-kernel mainlobe shape before picking the peak was expected
  to suppress single-bin noise spikes relative to a genuine
  `~zero_pad`-bin-wide peak. Measured result: `found_right_peak` with
  `use_mf=True` is equal to or slightly WORSE than without it at every
  Es/N0/collection-length combination tried (e.g. 2 dB/300 epochs:
  0.25 -> 0.17). Plausible reason: the squared-signal's noise
  cross-terms are themselves correlated across nearby bins (same
  finite-length DFT), so locally summing bins doesn't discriminate
  signal from this particular noise the way it would for independent
  per-bin noise -- an honest negative result, kept in the code as an
  opt-in (`use_mf=False` default) rather than deleted, in case a
  different kernel shape or normalization is worth trying later, but
  NOT recommended as-is. Multi-look averaging is the lever that
  actually works.
"""
from __future__ import annotations

import numpy as np

from doppler.acquire import CarrierAcquisition
from doppler.resample import RateConverter
from doppler.spectral import FFT

_NO_TEMPLATE = np.array([], dtype=np.float32)


def _parabolic_peak(power, k):
    """Sub-bin refinement of a DFT power-spectrum peak at integer bin
    `k`, via the standard 3-point parabolic (quadratic) fit through
    `power[k-1], power[k], power[k+1]` (indices wrap circularly, valid
    for a full-period DFT). Returns the fractional bin OFFSET from `k`
    (in `[-0.5, 0.5]`); add it to `k` for the refined bin position.
    Degenerates to 0 if the three points are exactly collinear
    (flat/noise-floor peak) rather than dividing by zero.
    """
    n = len(power)
    y1, y2, y3 = power[(k - 1) % n], power[k], power[(k + 1) % n]
    denom = y1 - 2.0 * y2 + y3
    if denom == 0.0:
        return 0.0
    return 0.5 * (y1 - y3) / denom


def _dirichlet_mainlobe_kernel(n_fft, zero_pad):
    """The KNOWN magnitude-squared Dirichlet kernel of a length-`n_fft`
    rectangular window, sampled at the zero-padded bin spacing, local
    to its mainlobe only (first null at +-`zero_pad` padded bins --
    `theta_null = 2*pi/n_fft` in continuous angular frequency, and each
    padded bin steps `2*pi/(n_fft*zero_pad)` -- so the null lands
    exactly `zero_pad` bins out). Returned as `(offsets, weights)`,
    `weights` peak-normalized to 1 at `offsets=0`, covering out to the
    first sidelobe (`2*zero_pad` bins) so the matched filter has a
    little margin beyond the bare mainlobe.
    """
    half_width = max(1, 2 * zero_pad)
    offsets = np.arange(-half_width, half_width + 1)
    theta = 2.0 * np.pi * offsets / (n_fft * zero_pad)
    with np.errstate(divide="ignore", invalid="ignore"):
        weights = np.where(
            offsets == 0,
            1.0,
            (np.sin(n_fft * theta / 2.0) / (n_fft * np.sin(theta / 2.0)))
            ** 2,
        )
    return offsets, weights


def _matched_filter_power(power, n_fft, zero_pad):
    """Cross-correlate `power` (an accumulated `|FFT|^2` spectrum)
    against the KNOWN Dirichlet mainlobe shape (see
    `_dirichlet_mainlobe_kernel`) -- a local weighted sum around each
    candidate bin, not a raw per-bin `argmax`. A genuine tone's energy
    is spread across `~zero_pad` bins in exactly this shape (zero-pad
    doesn't add resolution, it samples this same kernel more finely);
    a single isolated noise spike is not, so this suppresses spurious
    single-bin peaks relative to a real one -- closer to the actual
    maximum-likelihood statistic for a tone in white noise than raw
    per-bin energy.
    """
    n = len(power)
    offsets, weights = _dirichlet_mainlobe_kernel(n_fft, zero_pad)
    mf = np.zeros(n)
    for offset, weight in zip(offsets, weights):
        mf += weight * np.roll(power, -offset)
    return mf


def estimate_residual_freq(
    x, sample_rate_hz, n_fft=64, zero_pad=1, interp=True, use_mf=False,
):
    """Estimate a residual carrier tone's frequency (Hz) in a
    BPSK-data-modulated complex sequence `x`, sampled at
    `sample_rate_hz`.

    Squares `x` (removes the +-1 data modulation, doubling the tone's
    own frequency), then non-coherently accumulates `|FFT|^2` over
    `len(x) // n_fft` blocks of length `n_fft` (each optionally
    zero-padded to `n_fft * zero_pad` before its FFT) and returns the
    halved peak-bin frequency, optionally sub-bin refined via a
    3-point parabolic fit around the peak.

    Parameters
    ----------
    x : ndarray, complex
        Despread, window-rate output (e.g. `CoupledAsyncDespreader
        .run()`'s return value, collected with `freeze_carrier=True`
        so the residual tone isn't already being chased by a closed
        loop).
    sample_rate_hz : float
        Sample rate of `x` (the despread stream's own window rate,
        NOT the chip/sample rate of the original raw signal).
    n_fft : int
        Coherent block length. The REAL frequency resolution
        (pre-halving) is fixed at `sample_rate_hz / n_fft` by this
        block's own time duration, regardless of `zero_pad`; more
        blocks (longer `x`) improve the noise floor of the
        non-coherent sum without changing resolution.
    zero_pad : int
        Zero-pad each block to `n_fft * zero_pad` samples before its
        FFT. Densifies the sampled spectrum (reduces picket-fence
        bias) without adding real resolution; `1` disables it
        (original coarse-bin behavior).
    interp : bool
        Apply a 3-point parabolic sub-bin refinement around the
        (possibly matched-filtered) peak. Only sharpens a
        correctly-found peak -- see the module docstring's note on
        gross errors at low Es/N0.
    use_mf : bool
        Cross-correlate the accumulated power spectrum against the
        KNOWN Dirichlet mainlobe shape (`_matched_filter_power`)
        before picking the peak -- suppresses single-bin noise spikes
        relative to a genuine multi-bin-wide peak, aimed directly at
        low-Es/N0 gross errors (unlike `zero_pad`/`interp`, which only
        sharpen an already-correct peak).

    Returns
    -------
    float
        Estimated residual frequency in Hz, signed, at `x`'s own rate
        (same sign convention as the tone actually present in `x`).

    Examples
    --------
    >>> import numpy as np
    >>> n = 4096
    >>> rng = np.random.default_rng(0)
    >>> data = np.where(rng.integers(0, 2, n), 1.0, -1.0)
    >>> tone = np.exp(2j * np.pi * 123.0 * np.arange(n) / 2000.0)
    >>> est = estimate_residual_freq(
    ...     data * tone, 2000.0, n_fft=64, zero_pad=4,
    ... )
    >>> bool(abs(est - 123.0) < 5.0)  # sub-bin accurate now
    True
    """
    n_blocks = len(x) // n_fft
    if n_blocks < 1:
        raise ValueError(
            f"len(x)={len(x)} shorter than n_fft={n_fft}: need at "
            "least one coherent block"
        )
    n_padded = n_fft * zero_pad
    squared = (x[: n_blocks * n_fft] ** 2).astype(np.complex128)
    fft = FFT(n=n_padded, sign=-1)
    power = np.zeros(n_padded)
    padded_block = np.zeros(n_padded, dtype=np.complex128)
    for i in range(n_blocks):
        padded_block[:n_fft] = squared[i * n_fft:(i + 1) * n_fft]
        if zero_pad > 1:
            padded_block[n_fft:] = 0.0
        spec = fft.execute_cf64(padded_block)
        power += np.abs(spec) ** 2

    decision_power = (
        _matched_filter_power(power, n_fft, zero_pad) if use_mf else power
    )
    peak_bin = int(np.argmax(decision_power))
    bin_pos = peak_bin
    if interp:
        bin_pos = peak_bin + _parabolic_peak(decision_power, peak_bin)
        # keep the physically-meaningful wrap (fftfreq's own
        # convention: bins > n/2 are negative frequencies) even after
        # a fractional refinement that could cross that boundary.
        if bin_pos > n_padded / 2:
            bin_pos -= n_padded

    squared_freq_hz = bin_pos * sample_rate_hz / n_padded
    return squared_freq_hz / 2.0


def _collect_frozen_carrier_prefix(
    tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
    sample_rate_hz, bn_car, windows, init_chip,
):
    """Shared collection pass behind both `refine_seed` and
    `refine_seed_matched`: run one frozen-carrier despread pass
    (`freeze_carrier=True`, `aid_code=False` -- isolates the collection
    from any closed-loop carrier adaptation, letting the CODE loop lock
    on its own while the carrier NCO holds at `seeded_norm_freq`) over
    `rx_prefix`, and return `(out0, window_rate_hz)` -- the despread
    "windows"-rate output stream and its own sample rate, ready for
    either estimator to consume. Factored out so the two frequency
    estimators below (squaring-based vs. template-matched) share
    identical collection semantics and can be compared apples-to-apples
    -- see `refine_seed`'s own docstring for why `init_chip` matters.
    """
    d0 = tracker_cls(
        code, sps, bn=bn, zeta=0.707, windows=windows, bn_car=bn_car,
        init_chip=init_chip, init_car_norm_freq=seeded_norm_freq,
        aid_code=False, sample_rate_hz=sample_rate_hz, freeze_carrier=True,
    )
    out0 = d0.run(rx_prefix)
    window_rate_hz = sample_rate_hz / d0.step_size
    return out0, window_rate_hz


def refine_seed(
    tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
    sample_rate_hz, n_fft=64, zero_pad=4, interp=True, use_mf=False,
    bn_car=None, windows=6, init_chip=0.0,
):
    """Run one frozen-carrier collection pass (`freeze_carrier=True`,
    `aid_code=False` -- isolates the collection from any closed-loop
    adaptation) over `rx_prefix`, estimate the residual carrier tone
    left after derotating at `seeded_norm_freq`, and return the
    refined seed (`seeded_norm_freq` plus the estimated residual,
    converted back to cycles/sample) ready to construct a fresh
    tracker with.

    `tracker_cls` is `CoupledAsyncDespreader` (passed in, not
    imported, to keep this module import-independent of the
    coupled-tracker prototype file per this folder's own precedent of
    self-contained sibling modules).

    `init_chip` seeds the collection tracker's own code phase (chips)
    -- every caller before `acq_handoff.py` always collected at code
    phase 0 (a hand-chosen residual with no real code-phase handoff),
    so this defaulted to 0.0 implicitly and no caller needed to pass
    it. A real `Acquisition`-detected code phase is generally NONZERO;
    without seeding it here, `d0`'s own code loop starts badly
    mismatched against the real code phase in `rx_prefix`, degrading
    the despread output enough to corrupt the frequency estimate
    (found the hard way validating against `SPEC.md`'s real operating
    point -- see `acq_handoff.py`'s module docstring).
    """
    out0, window_rate_hz = _collect_frozen_carrier_prefix(
        tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
        sample_rate_hz, bn_car, windows, init_chip,
    )
    residual_hz = estimate_residual_freq(
        out0, window_rate_hz, n_fft=n_fft, zero_pad=zero_pad,
        interp=interp, use_mf=use_mf,
    )
    refined_norm_freq = seeded_norm_freq + residual_hz / sample_rate_hz
    return refined_norm_freq, residual_hz


def _known_symbol_psd_template(n_padded, sample_rate_hz, symbol_rate_hz):
    """The KNOWN (analytically precomputed, not empirically estimated)
    power spectral density shape of a random +-1 rectangular-pulse
    (NRZ) symbol stream at `symbol_rate_hz`, DC-centered, sampled at
    the SAME DFT bin grid (`n_padded` bins spanning `sample_rate_hz`)
    the measured periodogram uses so the two are directly comparable.

    A random bipolar rectangular pulse train has a closed-form
    continuous-time PSD `S(f) = sinc^2(f / symbol_rate_hz)` (first null
    at +-`symbol_rate_hz`) -- this is the average spectral shape of the
    DESPREAD data-modulated stream (the spreading code has already been
    removed by this point; only the +-1 data symbols remain), known
    exactly from the waveform's own symbol rate regardless of what the
    actual (unknown, random) data bits are.
    """
    freqs = np.fft.fftfreq(n_padded, d=1.0 / sample_rate_hz)
    return np.sinc(freqs / symbol_rate_hz) ** 2


def _template_correlate(power, template):
    """Circular cross-correlation of the measured `power` spectrum
    against the known `template` shape, evaluated at every candidate
    frequency-shift bin `k`: `corr[k] = sum_m power[m] * template[(m -
    k) mod n]` -- i.e. how well the template, shifted by `k` bins,
    explains the measured spectrum. `argmax(corr)` is the quasi-ML
    frequency-shift estimate when the underlying data symbols are
    unknown/random (their known AVERAGE spectral shape stands in for
    the unknown realization, rather than squaring them away at the
    cost of squaring loss)."""
    n = len(power)
    corr = np.empty(n)
    for k in range(n):
        corr[k] = np.sum(power * np.roll(template, k))
    return corr


def estimate_residual_freq_matched(
    x, sample_rate_hz, symbol_rate_hz, n_fft=64, zero_pad=4, interp=True,
):
    """Quasi-maximum-likelihood residual-carrier estimate for a
    BPSK-data-modulated complex sequence `x` with UNKNOWN (random) data
    symbols, avoiding `estimate_residual_freq`'s squaring step and its
    associated squaring loss entirely.

    Rather than squaring `x` to collapse the +-1 data ambiguity into a
    clean tone (halving the frequency and paying squaring loss --
    `[[reference_squaring_loss_ebno_not_cno]]` -- which is the direct
    cause of `estimate_residual_freq`'s gross wrong-peak errors near
    the noise floor), this treats the unknown data as a nuisance
    parameter and uses its KNOWN AVERAGE power spectral shape (a sinc^2
    for rectangular NRZ, `_known_symbol_psd_template`) directly: the
    measured (non-coherently accumulated, same as
    `estimate_residual_freq`) periodogram of the RAW, unsquared stream
    is cross-correlated against that known template at every candidate
    frequency shift, and the `argmax` of that correlation is the
    estimate -- the quasi-ML estimator when data symbols are unknown/
    random (as opposed to a known-pilot ML estimator, which would
    correlate against the actual known symbol sequence instead of its
    average shape). No squaring means no factor-of-2 undoing either --
    the returned Hz value is the residual frequency directly.

    Parameters mirror `estimate_residual_freq` except `symbol_rate_hz`
    (needed to build the known template) replaces `use_mf` (not
    applicable here -- the "matched filter" IS the known symbol PSD
    shape, not an FFT-window-artifact kernel).

    Examples
    --------
    >>> import numpy as np
    >>> sps = 20
    >>> n = 8192 // sps * sps  # exact multiple of sps
    >>> rng = np.random.default_rng(0)
    >>> sym_rate = 100.0
    >>> data = np.repeat(
    ...     np.where(rng.integers(0, 2, n // sps), 1.0, -1.0), sps,
    ... )
    >>> tone = np.exp(2j * np.pi * 123.0 * np.arange(n) / 2000.0)
    >>> est = estimate_residual_freq_matched(
    ...     data * tone, 2000.0, sym_rate, n_fft=256, zero_pad=4,
    ... )
    >>> bool(abs(est - 123.0) < 5.0)
    True
    """
    n_blocks = len(x) // n_fft
    if n_blocks < 1:
        raise ValueError(
            f"len(x)={len(x)} shorter than n_fft={n_fft}: need at "
            "least one coherent block"
        )
    n_padded = n_fft * zero_pad
    fft = FFT(n=n_padded, sign=-1)
    power = np.zeros(n_padded)
    padded_block = np.zeros(n_padded, dtype=np.complex128)
    for i in range(n_blocks):
        padded_block[:n_fft] = x[i * n_fft:(i + 1) * n_fft]
        if zero_pad > 1:
            padded_block[n_fft:] = 0.0
        spec = fft.execute_cf64(padded_block)
        power += np.abs(spec) ** 2

    template = _known_symbol_psd_template(
        n_padded, sample_rate_hz, symbol_rate_hz
    )
    corr = _template_correlate(power, template)
    peak_bin = int(np.argmax(corr))
    bin_pos = peak_bin
    if interp:
        bin_pos = peak_bin + _parabolic_peak(corr, peak_bin)
        if bin_pos > n_padded / 2:
            bin_pos -= n_padded

    return bin_pos * sample_rate_hz / n_padded


def refine_seed_matched(
    tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
    sample_rate_hz, symbol_rate_hz, n_fft=64, zero_pad=4, interp=True,
    bn_car=None, windows=6, init_chip=0.0,
):
    """`refine_seed`'s own collection pass, but estimating the residual
    with `estimate_residual_freq_matched` (known-PSD template
    correlation) instead of `estimate_residual_freq` (squaring). See
    `estimate_residual_freq_matched`'s own docstring for why this is
    expected to do better at low Es/N0 -- no squaring loss."""
    out0, window_rate_hz = _collect_frozen_carrier_prefix(
        tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
        sample_rate_hz, bn_car, windows, init_chip,
    )
    residual_hz = estimate_residual_freq_matched(
        out0, window_rate_hz, symbol_rate_hz, n_fft=n_fft,
        zero_pad=zero_pad, interp=interp,
    )
    refined_norm_freq = seeded_norm_freq + residual_hz / sample_rate_hz
    return refined_norm_freq, residual_hz


def refine_seed_carrier_acq(
    tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
    sample_rate_hz, symbol_rate_hz, cn0_dbhz, n_fft=64, zero_pad=8,
    bn_car=None, windows=6, init_chip=0.0, pfa=1e-3, pd=0.9,
    design_margin_db=14.0, sequential=False, max_n_blocks=100000,
    samples_per_symbol=4,
):
    """`refine_seed_matched`'s own collection pass, but estimating the
    residual with the C-backed `doppler.acquire.CarrierAcquisition` (a
    jm port of this same known-PSD-template correlation) instead of
    the Python `estimate_residual_freq_matched`. This is the fold-in
    the user asked for once `CarrierAcquisition`'s own CFAR threshold
    was fixed (`carrier_acq_core.c`'s `_ratio_threshold()` -- the
    borrowed `det_threshold_noncoherent` model was ~5x too conservative
    for this statistic; see `FINISHING_PLAN.md`'s `CarrierAcquisition`
    section for the full derivation and the fix).

    `cn0_dbhz` is a DESIGN assumption (not a live measurement -- same
    role as `Acquisition`'s own `cn0_dbhz`), typically SPEC's own
    minimum-Es/N0 operating point converted via
    `spec_full_characterization.es_n0_to_cn0_dbhz`. `design_snr` is
    computed HERE from it, at the DESPREAD stream's own
    `window_rate_hz` -- NOT the raw chip rate `sample_rate_hz` this
    function otherwise takes -- because this estimator runs on the
    despread, window-rate output (`out0`), whose per-sample amplitude
    SNR is boosted by the despreading + window-averaging processing
    gain over the raw chip-rate stream. Same `amp_snr =
    sqrt(10**(cn0_dbhz/10)/fs)` convention `Acquisition` itself and
    every sibling demo in `src/doppler/examples/` already use, just
    evaluated at this stream's own rate.

    `design_margin_db` (default 14.0, empirical) compensates for a
    real, separate gap this session found and left open: `_ratio_
    threshold()`'s own KAPPA correction fixed the CFAR gate's
    detection DECISION for this object's real statistic, but
    `carrier_acq_create()`'s `dwell_target` PLANNING still calls
    `det_n_noncoh()` against the un-corrected classic model -- so the
    literal `cn0_dbhz`-derived `design_snr` predicts a dwell far
    smaller (e.g. 7 blocks at SPEC's own 5dB minimum) than what the
    real, KAPPA-corrected gate needs to actually fire reliably (in the
    tens-of-Hz-error range only once dwell reaches the many hundreds,
    confirmed against this folder's own real captures). Derating the
    design point by this fixed margin before conversion is a practical
    workaround, not a fix to that planning/testing model mismatch --
    see `FINISHING_PLAN.md`'s `CarrierAcquisition` section.

    `samples_per_symbol` (default 4) sets `CarrierAcquisition`'s OWN
    operating rate -- `target_rate_hz = samples_per_symbol *
    symbol_rate_hz` -- fully DECOUPLED from `windows`/`window_rate_hz`
    (the code-tracking loop's own async-lookback granularity, now
    itself derived from `despreader_coupled.async_lookback_windows()`,
    not sized for this estimator at all). The despreader's own
    integrate-and-dump (`find_max_power`'s per-window coherent sum)
    still does the heavy coherent averaging -- it is NOT relied on to
    also decimate down to this estimator's target rate; a proper
    anti-aliased `RateConverter` resample does that job explicitly,
    right before `CarrierAcquisition` ever sees the stream. Acquisition
    has already narrowed the residual to +/-(chip_rate/sf)/2 (this
    waveform: +/-1500 Hz) by the time this runs, and 4 samples/symbol
    gives a comfortable Nyquist margin over that (`2*samples_per_symbol
    *symbol_rate_hz` half-span) without oversampling far beyond what's
    needed, unlike inheriting whatever rate the code loop's own
    `windows` granularity happens to produce.

    `resolution_hz` is derived from `n_fft`/`target_rate_hz` (not the
    native `window_rate_hz`) so the coherent block length matches
    `estimate_residual_freq_matched`'s own `n_fft` in SYMBOL units
    (`n_fft=64` at `samples_per_symbol=4` -> 16 symbols/block, closer to
    the sinc^2 template's own "average PSD of a random NRZ stream"
    premise than a sub-symbol block would give). `zero_pad=8` lands on
    this object's own validated `nfft=512` calibration point.

    Returns `(refined_norm_freq, residual_hz, ready, samples_consumed)`.
    `ready` is `False` if `CarrierAcquisition` never fired (hit its
    give-up cap without a detection); `residual_hz`/`refined_norm_freq`
    are then whatever the object's own last (zeroed, if it never ran a
    single test) state holds, NOT a silently-reused prior estimate --
    callers MUST check `ready` before trusting them. `samples_consumed`
    is `ca.n_blocks`' worth of `rx_prefix`, converted back to raw
    `sample_rate_hz` samples -- callers MUST use THIS (not the full
    length of `rx_prefix` handed in) to decide where downstream tracking
    should resume: since `CarrierAcquisition`'s own dwell is now sized
    off its OWN calibrated statistic rather than a generous fixed
    prefix, it typically finishes using only a small fraction of
    whatever `rx_prefix` was supplied -- treating the REST of
    `rx_prefix` as already "spent" (as this project's own e2e harness
    did before this fix) hands a stale seed to the tracker, off by
    however much the residual has genuinely drifted in the unused
    remainder (confirmed directly: ~300+ Hz of avoidable error at
    `SPEC.md`'s own 500 Hz/s rate, entirely a stale-reference artifact,
    not an estimation error).
    """
    out0, window_rate_hz = _collect_frozen_carrier_prefix(
        tracker_cls, code, sps, bn, seeded_norm_freq, rx_prefix,
        sample_rate_hz, bn_car, windows, init_chip,
    )
    target_rate_hz = samples_per_symbol * symbol_rate_hz
    out0 = RateConverter(rate=target_rate_hz / window_rate_hz).execute(
        out0.astype(np.complex64)
    )
    effective_cn0_dbhz = cn0_dbhz - design_margin_db
    design_snr = float(
        np.sqrt(10.0 ** (effective_cn0_dbhz / 10.0) / target_rate_hz)
    )
    resolution_hz = target_rate_hz / n_fft
    ca = CarrierAcquisition(
        _NO_TEMPLATE, target_rate_hz, symbol_rate_hz,
        resolution_hz=resolution_hz, zero_pad=zero_pad, pfa=pfa, pd=pd,
        design_snr=design_snr, sequential=sequential,
        max_n_blocks=max_n_blocks,
    )
    ca.steps(out0.astype(np.complex64))
    residual_hz = ca.residual_hz
    ready = ca.ready
    refined_norm_freq = seeded_norm_freq + residual_hz / sample_rate_hz
    elapsed_s = ca.n_blocks * n_fft / target_rate_hz
    samples_consumed = int(round(elapsed_s * sample_rate_hz))
    return refined_norm_freq, residual_hz, ready, samples_consumed
