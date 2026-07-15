# dsss/dsss.pyi — type stubs for the dsss C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

class Despreader:
    """Create a despreader (COPIES code).

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        code constructor parameter.
    sps : int, default 4
        sps constructor parameter.
    init_norm_freq : float, default 0.0
        init_norm_freq constructor parameter.
    init_chip : float, default 0.0
        init_chip constructor parameter.
    bn_carrier : float, default 0.05
        bn_carrier constructor parameter.
    bn_code : float, default 0.005
        bn_code constructor parameter.
    bn_fll : float, default 0.0
        bn_fll constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    spacing : float, default 0.5
        spacing constructor parameter.
    periods_per_bit : int, default 1
        periods_per_bit constructor parameter.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sps: int = ..., init_norm_freq: float = ..., init_chip: float = ..., bn_carrier: float = ..., bn_code: float = ..., bn_fll: float = ..., zeta: float = ..., spacing: float = ..., periods_per_bit: int = ...) -> None: ...

    # jm:hand
    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = ...,
    ) -> NDArray[np.complex64]:
        """Track carrier + code and despread a cf32 block: per sample wipe the carrier (Costas) and correlate early/prompt/late against the code (DLL), update both loops each code period, and emit one complex prompt symbol per period.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see steps_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least max(steps_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    # jm:hand
    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state. Use to size the ``out=`` buffer."""

    # jm:hand
    def bits(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Same tracking kernel as steps(), but bit-sync the per-period prompts into hard data bits: periods_per_bit prompts are coherently summed across each detected bit boundary and one 0/1 bit is emitted per data bit.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see bits_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.uint8], optional
            Caller-provided output buffer, at least max(bits_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.uint8]
            Output.
        """

    # jm:hand
    def bits_max_out(self) -> int:
        """Max output length bits() can produce for the current state. Use to size the ``out=`` buffer."""

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context across the despreader. Pure forwarder — the despreader registers no probes of its own: the carrier loop registers "<prefix>.car.lock" / ".e" / ".freq" / ".locked" and the code loop registers "<prefix>.code.e" / ".rate" / ".lock" / ".locked" (the ".locked" pair are the loops' verify-counted lockdet decisions, 0/1) — eight probes, all thinned by decim and emitted once per code period (the despreader flushes both loops at its per-period update). Passing NULL detaches both loops.  Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in telemetry/telemetry.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "ch0".
        decim : int
            Emit every decim-th code period; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> code = (np.arange(31) % 2).astype(np.uint8)
        >>> ch = Despreader(code=code, sps=4)
        >>> ch.set_telemetry(tlm, "ch0")
        >>> names = sorted(tlm.probe_names())
        >>> names[:4]
        ['ch0.car.e', 'ch0.car.freq', 'ch0.car.lock', 'ch0.car.locked']
        >>> names[4:]
        ['ch0.code.e', 'ch0.code.lock', 'ch0.code.locked', 'ch0.code.rate']
        >>> chips = 1.0 - 2.0 * (np.arange(31) % 2)
        >>> x = np.tile(np.repeat(chips, 4), 40).astype(np.complex64)
        >>> _ = ch.steps(x)
        >>> recs = tlm.read()   # eight records per code period
        >>> len(recs) > 0 and len(recs) % 8 == 0
        True

        """

    def configure_carrier_lock(self, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Re-tune the embedded carrier loop's lock detector directly: forwards to the Costas loop's configure_lock (locked flips up after n_up consecutive symbols with the lock-metric EMA above up_thresh, and drops after n_down consecutive symbols below down_thresh; see Costas.configure_lock). Symmetric with the carrier_locked state property: state is readable, so config should be writable too, rather than forcing a caller who needs this control to drop to raw Dll+Costas composition.

        Thin forwarder to costas_configure_lock() on the embedded Costas loop —
        symmetric with despreader_get_carrier_locked() exposing its state: state
        is readable, so config should be writable too, rather than forcing a
        caller who needs this control to drop to raw Dll+Costas composition
        instead of Despreader. See costas_configure_lock() for the parameter
        semantics.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the lock-metric EMA.
        down_thresh : float
            Drop threshold (<= up_thresh for level hysteresis).
        n_up : int
            Consecutive above-threshold symbols to declare.
        n_down : int
            Consecutive below-threshold symbols to drop.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
        >>> d.configure_carrier_lock(0.9, 0.8, 4, 16)  # tighter declare/drop

        """

    def configure_code_lock(self, pfa: float, n_looks: int, ref_snr_db: float = 0.0) -> None:
        """Re-tune the embedded code loop's lock detector: forwards to the DLL's configure_lock (see Dll.configure_lock) -- the derived (pfa-style) entry point, matching Despreader's role as the easy composed API (Dll's raw escape hatch, configure_lock_raw, stays a Dll-only control for a caller that composes Dll+Costas directly). Raises ValueError for pfa outside (0, 1).

        Thin forwarder to dll_configure_lock() on the embedded DLL — the derived
        (pfa-style) entry point, matching Despreader's role as the "easy"
        composed API (Dll's raw escape hatch, dll_configure_lock_raw(), stays a
        Dll-only control for a caller that composes Dll+Costas directly). See
        dll_configure_lock() for the parameter semantics.

        Parameters
        ----------
        pfa : float
            Per-decision false-alarm probability, in (0, 1).
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        ref_snr_db : float
            Noise-reference estimator SNR in dB (> 0), or 0 to derive from n_looks (see dll_configure_lock()).

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.dsss import Despreader
        >>> d = Despreader(code=np.zeros(31, dtype=np.uint8), sps=2)
        >>> d.configure_code_lock(1e-3, 20)
        >>> d.code_locked
        False
        >>> d.configure_code_lock(2.0, 20)
        Traceback (most recent call last):
            ...
        ValueError: configure_code_lock failed (rc=-4)

        """

    def reset(self) -> None:
        """Re-seed both loops to the create-time frequency/phase; preserve config.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Code phase."""

    @property
    def code_rate(self) -> float:
        """Code rate."""

    @property
    def lock_metric(self) -> float:
        """Lock metric."""

    @property
    def carrier_locked(self) -> bool:
        """Carrier lock decision: the embedded Costas loop's verify-counted detector on its lock-metric EMA (True = locked; see Costas.configure_lock)."""

    @property
    def code_locked(self) -> bool:
        """Code lock decision: the embedded DLL's verify-counted CFAR detector (True = locked; see Dll.configure_lock). Live in composition — the despreader runs the same always-on detector Dll.steps does."""

    @property
    def bit_phase(self) -> int:
        """Bit phase."""

    @property
    def bn_carrier(self) -> float:
        """Bn carrier."""
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...

    @property
    def bn_code(self) -> float:
        """Bn code."""
    @bn_code.setter
    def bn_code(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Despreader": ...

    def __exit__(self, *args: object) -> None: ...

class BurstDespreader:
    """Create a burst despreader instance.

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        Data spreading code (0/1 chips), length code_len; copied.
    sf : int, default 1
        Spreading factor: chips integrated per prompt symbol (default: 1).
    sps : int, default 2
        Samples per chip (default: 2).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample — the acquisition estimate (default: 0.0).
    init_chip_phase : float, default 0.0
        Seed code phase, chips (default: 0.0).
    bn_carrier : float, default 0.05
        Carrier (Costas) loop noise bandwidth, normalized to the symbol rate (default: 0.05).
    bn_code : float, default 0.01
        Code (DLL) loop noise bandwidth, normalized to the symbol rate (default: 0.01).

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sf: int = ..., sps: int = ..., init_norm_freq: float = ..., init_chip_phase: float = ..., bn_carrier: float = ..., bn_code: float = ...) -> None: ...

    # jm:hand
    def steps(
        self,
        x: NDArray[np.complex64],
        out: NDArray[np.complex64] | None = ...,
    ) -> NDArray[np.complex64]:
        """Despread a cf32 block; emit one complex prompt symbol per code period.

        Streams: a partial symbol is carried in state across calls. Each emitted
        symbol is the complex prompt integrate-and-dump (carrier-wiped,
        code-stripped) — its sign is the BPSK decision, its phase/magnitude the
        soft information. During a `burst_despreader_set_acq` preamble no symbols are
        emitted (the loops are pulling in); payload symbols follow.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see steps_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input CF32 samples, length x_len.
        out : NDArray[np.complex64], optional
            Caller-provided output buffer, at least max(steps_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.

        Examples
        --------
        // seed from acquisition (norm_freq cyc/sample, chip phase in chips):
        burst_despreader_state_t *d = burst_despreader_create(code, n, 32, 2, f0, chip, .05, .01);
        float complex sym[256];
        size_t k = burst_despreader_steps(d, rx, rx_len, sym, 256);
        // hard bit of sym[i] = crealf(sym[i]) >= 0
        burst_despreader_destroy(d);

        """

    # jm:hand
    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state. Use to size the ``out=`` buffer."""

    # jm:hand
    def bits(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Despread a cf32 block; emit one hard BPSK bit per code period.

        Same streaming kernel as burst_despreader_steps(), but emits the hard decision
        `crealf(prompt) >= 0` instead of the complex symbol.

        Without out=, the returned array is a view into a buffer reused on the
        next call (see bits_max_out() to size an out= buffer for an
        independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input CF32 samples, length x_len.
        out : NDArray[np.uint8], optional
            Caller-provided output buffer, at least max(bits_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.
        """

    # jm:hand
    def bits_max_out(self) -> int:
        """Max output length bits() can produce for the current state. Use to size the ``out=`` buffer."""

    def set_acq(self, acq_code: NDArray[np.uint8], acq_reps: int) -> None:
        """Enable preamble-aided pull-in: track acq_reps periods of the (distinct) acq_code coherently before despreading the payload with the data code. Call before feeding the burst; clears when the preamble is consumed.

        Track acq_reps periods of acq_code coherently (the unmodulated, repeated
        acquisition preamble — a full ±pi phase discriminator, so the loops pull
        in even a wide residual) before switching to the data code for the
        payload. Call before feeding the burst; the acq mode clears
        automatically once the preamble is consumed, and re-arms on
        burst_despreader_reset(). NB: set_acq re-arms the PREAMBLE only — the
        cumulative burst statistics (lock_metric / snr_est / lock_stat / stat_n)
        are re-armed by burst_despreader_reset(); call it between bursts.

        Parameters
        ----------
        acq_code : NDArray[np.uint8]
            Acquisition code (0/1), length acq_code_len; copied.
        acq_reps : int
            Number of acq-code periods in the preamble.
        """

    def reset(self) -> None:
        """Re-seed the loops to the create-time phase/frequency; preserve config.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def bn_carrier(self) -> float:
        """Carrier (Costas) loop noise bandwidth, normalized to the symbol rate."""
    @bn_carrier.setter
    def bn_carrier(self, value: float) -> None: ...

    @property
    def bn_code(self) -> float:
        """Code (DLL) loop noise bandwidth, normalized to the symbol rate."""
    @bn_code.setter
    def bn_code(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Current carrier frequency estimate, cycles/sample."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Current tracked code phase within the symbol, chips."""

    @property
    def lock_metric(self) -> float:
        """Lock indicator in [0,1]: the mean of |Re prompt|/|prompt| over every prompt of the burst (cumulative, not EMA). ~1 when phase-locked; ~2/pi (0.637) with no carrier."""

    @property
    def snr_est(self) -> float:
        """Post-despread SNR estimate over the burst, accumulate-then-ratio: (sum Re^2 - sum Im^2)/sum Im^2, clamped >= 0. This is the effective post-loop SNR (residual tracking jitter included) - the quantity that predicts demodulation performance; it converges to the AWGN-only A^2/sigma^2 as the loop bandwidths shrink."""

    @property
    def lock_stat(self) -> float:
        """Calibrated whole-burst lock statistic R = sqrt(stat_n * sum Re^2 / sum Im^2) — the one-shot analog of the tracking loops' verify-counted detectors. Because the noise reference is estimated from as many samples as the signal sum, the exact H0 law is R^2 = stat_n * F(stat_n, stat_n): gate with R > sqrt(stat_n * det_threshold_f(pfa, stat_n)) — exact for every stat_n (a chi-square gate would realize tens of times the priced pfa). Payload prompts only; reset() re-arms."""

    @property
    def stat_n(self) -> int:
        """Number of prompts folded into the burst statistics so far."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "BurstDespreader": ...

    def __exit__(self, *args: object) -> None: ...

class Acquisition:
    """Create a streaming DSSS acquisition engine.

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
        PN chips (0/1), length code_len.
    reps : int, default 1
        Max coherent code repetitions, the coherence ceiling (>=1).
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    cn0_dbhz : float, default 50.0
        Carrier-to-noise density in dB-Hz (> 0).
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz; 0 uses the full native span +/- chip_rate/(2*sf).  Must be <= span.
    pfa : float, default 1e-3
        Target system (max-of-N) false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.
    max_noncoh : int, default 1
        Cap on the auto-split non-coherent look count (>= 1; default 1 keeps the engine purely coherent).
    symbol_rate : float, default 0.0
        Continuous data-symbol rate in Hz; <= 0 (default) disables the data-modulation-aware search above.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., reps: int = ..., spc: int = ..., chip_rate: float = ..., cn0_dbhz: float = ..., doppler_uncertainty: float = ..., pfa: float = ..., pd: float = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean", max_noncoh: int = ..., symbol_rate: float = ...) -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.
        """

    def push(self, x: complex) -> list[tuple[int, int, float, float, float, float]]:
        """Stream raw samples; emit one event per CFAR dump above threshold.

        Buffers in, then for every complete frame applies the slow-time Doppler
        FFT, correlates against the PN reference, dumps the coherent surface
        (or, when n_noncoh > 1, accumulates |·|² over n_noncoh looks first),
        gates the peak on the auto-configured threshold, and appends an
        acq_result_t.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, int, float, float, float, float]]
            Number of events written (0 … max_results).
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches -- the advanced escape hatch (mirrors Dll.configure_lock_raw/Costas.configure_lock). Resizes every buffer/plan that depends on the grid (the slow-time FFT, the code correlator, the reference, and every per-frame scratch buffer), re-derives the threshold ladder for the pinned grid from the same physics __init__ used, and clears in-flight accumulation (ring contents, the non-coherent power accumulator, dwell bookkeeping) -- call between push() calls, never a substitute for one. Raises ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside [1, max_noncoh].

        Resizes every buffer/plan that depends on the grid (the slow-time FFT,
        the code correlator, the reference, and every per-frame scratch buffer),
        re-derives the threshold ladder for the pinned grid from the same
        physics acq_create() used, and clears in-flight accumulation (ring
        contents, the non-coherent power accumulator, dwell bookkeeping) — call
        between push() calls, never a substitute for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1, max_noncoh]`.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def code_bins(self) -> int:
        """Code-phase hypotheses searched (= sf*spc, one code period)."""

    @property
    def doppler_bins(self) -> int:
        """Coherent depth chosen: the slow-time FFT length in code reps (<= reps)."""

    @property
    def sf(self) -> int:
        """Chips per PN segment, inferred from len(code)."""

    @property
    def spc(self) -> int:
        """Samples per chip (chip-rate oversample factor)."""

    @property
    def reps(self) -> int:
        """Max coherent code repetitions (the coherence ceiling)."""

    @property
    def n_noncoh(self) -> int:
        """Non-coherent looks per detection (1 = pure coherent)."""

    @property
    def ring_cap(self) -> int:
        """Input ring capacity in complex samples."""

    @property
    def noise_lo(self) -> int:
        """First CFAR reference bin (inclusive)."""

    @property
    def noise_hi(self) -> int:
        """Last CFAR reference bin (inclusive)."""

    @property
    def threshold(self) -> float:
        """CFAR gate on the test statistic (coherent path)."""

    @property
    def eta(self) -> float:
        """Raw per-cell Rayleigh amplitude threshold."""

    @property
    def eta_nc(self) -> float:
        """Non-coherent CFAR threshold (order-N_nc Marcum)."""

    @property
    def pfa_cell(self) -> float:
        """Bonferroni per-cell false-alarm probability over the searched cells."""

    @property
    def pd_predicted(self) -> float:
        """Predicted Pd at cn0_dbhz and the chosen grid: the average Pd over the straddle priors (slow-time scalloping, intra-segment rotation, code-phase sample offset - quadrature over uniform priors), matching what the Monte-Carlo characterization measures rather than the on-grid best case."""

    @property
    def straddle_loss(self) -> float:
        """Mean amplitude derating of the correlation peak from grid straddle (slow-time Doppler scalloping x intra-segment rotation x code-phase sample offset, each averaged over a uniform prior) - a diagnostic summary; 20*log10(straddle_loss) is the loss in dB. Sizing and pd_predicted average Pd itself over the priors (Pd at this mean amplitude would overstate the mean Pd)."""

    @property
    def fs(self) -> float:
        """Sample rate (Hz) = chip_rate * spc."""

    @property
    def chip_rate(self) -> float:
        """Chip rate (Hz)."""

    @property
    def cn0_dbhz(self) -> float:
        """Carrier-to-noise density used to size the search (dB-Hz)."""

    @property
    def doppler_span_hz(self) -> float:
        """Native unambiguous Doppler half-range = +/- chip_rate/(2*sf) Hz."""

    @property
    def doppler_res_hz(self) -> float:
        """Doppler bin width = chip_rate/(sf*doppler_bins) Hz."""

    @property
    def pd(self) -> float:
        """Target detection probability."""

    @property
    def underpowered(self) -> bool:
        """True when pd_predicted < pd (the search cannot meet the target)."""

    @property
    def symbol_rate(self) -> float:
        """Continuous data-symbol rate (Hz) used to size the search; 0 means the legacy Doppler/code-phase-only search (no known data-modulation clock)."""

    @property
    def epochs_per_symbol(self) -> float:
        """(chip_rate/sf)/symbol_rate -- code epochs per data symbol; 0 when symbol_rate is 0."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Acquisition": ...

    def __exit__(self, *args: object) -> None: ...

class PolynomialPhaseEstimator:
    """Create a polynomial-phase estimator.

    Parameters
    ----------
    max_len : int, default 4096
        Maximum input sequence length (>= 4).
    max_rate : float, default 0.0
        Chirp-rate search half-span (cycles/sample^2); 0 searches frequency only (a single FFT — near-static Doppler).

    Examples
    --------
    Create with defaults:

    >>> from doppler.dsss import PolynomialPhaseEstimator
    >>> obj = PolynomialPhaseEstimator(max_len=4096, max_rate=0.0)

    """
    def __init__(self, max_len: int = ..., max_rate: float = ...) -> None: ...

    def reset(self) -> None:
        """No-op (the estimator carries no running state).
        """

    def estimate(self, x: complex) -> tuple[float, float, float]:
        """Estimate (freq, chirp-rate) of a complex sequence via the 2-lag HAF.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        tuple[float, float, float]
            The estimate; zeroed if n_in is out of range.
        """

    @property
    def max_len(self) -> int:
        """Max len."""

    @property
    def nfft(self) -> int:
        """Nfft."""

    @property
    def max_rate(self) -> float:
        """Max rate."""

    @property
    def n_rate(self) -> int:
        """N rate."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "PolynomialPhaseEstimator": ...

    def __exit__(self, *args: object) -> None: ...

class BurstDemod:
    """Create a burst demodulator.

    Parameters
    ----------
    data_code : NDArray[np.uint8], default ...
        Data spreading code (0/1); copied.
    spc : int, default 4
        Samples per chip.
    chip_rate : float, default 1.0e6
        Chip rate (Hz).
    carrier_hz : float, default 0.0
        RF carrier (Hz) for code-Doppler scaling; 0 = ignore.
    max_rate : float, default 0.0
        Chirp-rate search half-span (cycles/sample^2 at the input rate); 0 = Doppler only (no rate search).
    payload_len : int, default 0
        Number of payload data symbols (bits) in a frame.
    est_segments : int, default 10
        Partial correlations per acq period (segmentation for the feedforward estimate; larger tolerates more rate).

    """
    def __init__(self, data_code: NDArray[np.uint8] = ..., spc: int = ..., chip_rate: float = ..., carrier_hz: float = ..., max_rate: float = ..., payload_len: int = ..., est_segments: int = ...) -> None: ...

    def reset(self) -> None:
        """Clear the read-backs (config is preserved).
        """

    def set_preamble(self, acq_code: NDArray[np.uint8], reps: int) -> None:
        """Set the (unmodulated) acquisition preamble code + repetition count used for the feedforward (f0, rate) estimate.

        Parameters
        ----------
        acq_code : NDArray[np.uint8]
            Input.
        reps : int
            Input.
        """

    def set_sync(self, sync: NDArray[np.uint8]) -> None:
        """Set the known frame-sync word (0/1 BPSK symbols) used for frame alignment + phase/sign resolution.

        Parameters
        ----------
        sync : NDArray[np.uint8]
            Input.
        """

    def set_prior(self, f0_coarse: float, start: int) -> None:
        """Seed from acquisition: coarse Doppler (cycles/sample at the input rate) and the preamble start sample.

        Parameters
        ----------
        f0_coarse : float
            Input.
        start : int
            Input.
        """

    # jm:hand
    def demod(
        self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = ...
    ) -> NDArray[np.uint8]:
        """Demodulate a burst (preamble + frame); return the payload bits. Read-back properties report the estimates + CRC validity.

        Without out=, the returned array is a view into a buffer reused on
        the next call (see demod_max_out(), or payload_len, to size an out=
        buffer for an independent, alias-free result).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.
        out : NDArray[np.uint8], optional
            Caller-provided output buffer, at least max(demod_max_out(),
            len(x)) elements.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written (0 on failure / too-short burst). The read-back fields (frame_valid, est_*, frame_offset) are updated.
        """

    # jm:hand
    def demod_max_out(self) -> int:
        """Max output length demod() can produce for the current state. Use to size the ``out=`` buffer."""

    @property
    def frame_valid(self) -> int:
        """Frame valid."""

    @property
    def frame_offset(self) -> int:
        """Frame offset."""

    @property
    def n_symbols(self) -> int:
        """N symbols."""

    @property
    def est_freq_hz(self) -> float:
        """Est freq hz."""

    @property
    def est_rate_hz(self) -> float:
        """Est rate hz."""

    @property
    def est_snr_db(self) -> float:
        """Est snr db."""

    @property
    def payload_len(self) -> int:
        """Payload len."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "BurstDemod": ...

    def __exit__(self, *args: object) -> None: ...
