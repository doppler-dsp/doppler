# dsss/dsss.pyi — type stubs for the dsss C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class Despreader:
    """Create a despreader (COPIES code).

    Parameters
    ----------
    code : NDArray[np.uint8]
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

@final
class BurstDespreader:
    """Create a burst despreader instance.

    Parameters
    ----------
    code : NDArray[np.uint8]
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

@final
class Acquisition:
    """Acquisition component.

    Parameters
    ----------
    code : NDArray[np.uint8]
        code constructor parameter.
    spc : int, default 4
        spc constructor parameter.
    chip_rate : float, default 1000000.0
        chip_rate constructor parameter.
    symbol_rate : float, default 1000.0
        symbol_rate constructor parameter.
    cn0_dbhz : float, default 50.0
        cn0_dbhz constructor parameter.
    doppler_uncertainty : float, default 0.0
        doppler_uncertainty constructor parameter.
    pfa : float, default 1e-3
        pfa constructor parameter.
    pd : float, default 0.9
        pd constructor parameter.
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        noise_mode constructor parameter.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., spc: int = ..., chip_rate: float = ..., symbol_rate: float = ..., cn0_dbhz: float = ..., doppler_uncertainty: float = ..., pfa: float = ..., pd: float = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean") -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator.
        """

    def push(self, x: complex) -> list[tuple[int, int, float, float, float, float, int]]:
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
        list[tuple[int, int, float, float, float, float, int]]
            Number of events written (0 … max_results).
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches -- the advanced escape hatch (mirrors Dll.configure_lock_raw/Costas.configure_lock). Resizes every buffer/plan that depends on the grid (the slow-time FFT, the code correlator, the reference, and every per-frame scratch buffer), re-derives the threshold ladder for the pinned grid from the same physics __init__ used, and clears in-flight accumulation (ring contents, the non-coherent power accumulator, dwell bookkeeping) -- call between push() calls, never a substitute for one. Raises ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside [1, 256] (the internal non-coherent-look safety-valve ceiling).

        Resizes every buffer/plan that depends on the grid (the slow-time FFT,
        the code correlator, the reference, and every per-frame scratch buffer),
        re-derives the threshold ladder for the pinned grid from the same
        physics acq_create_burst()/acq_create_continuous() used, and clears
        in-flight accumulation (ring contents, the non-coherent power
        accumulator, dwell bookkeeping) — call between push() calls, never a
        substitute for one.

        Parameters
        ----------
        doppler_bins : int
            Coherent depth to pin, in `[1, reps]`.
        n_noncoh : int
            Non-coherent look count to pin, in `[1, ACQ_N_NONCOH_SAFETY_CEILING]`.
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
        """Effective Doppler search granularity this engine picked: the window-tile count (this engine always window-tiles -- see acq_core.h's file doc comment -- so this is window_bins, never a coherent-depth axis)."""

    @property
    def sf(self) -> int:
        """Chips per PN segment, inferred from len(code)."""

    @property
    def spc(self) -> int:
        """Samples per chip (chip-rate oversample factor)."""

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
        """True when pd_predicted < pd -- the search cannot meet the target pd at this cn0_dbhz and geometry. The engine still builds a best-effort grid rather than failing; because C cannot raise a Python warning from a successful create, construction also emits a UserWarning in this case."""

    @property
    def symbol_rate(self) -> float:
        """Continuous data-symbol rate (Hz) this engine was built with -- diagnostic only, doesn't feed sizing (this engine never coherently combines regardless)."""

    @property
    def epochs_per_symbol(self) -> float:
        """(chip_rate/sf)/symbol_rate -- code epochs per data symbol; 0 when symbol_rate is 0."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Acquisition": ...

    def __exit__(self, *args: object) -> None: ...

@final
class BurstAcquisition:
    """Create a burst-mode acquisition engine (forwards to acq_create_burst() -- see its doc comment in acq_core.h for the full physics).

    Parameters
    ----------
    code : NDArray[np.uint8]
        PN chips (0/1), length code_len.
    reps : int, default 1
        Max coherent code repetitions (>= 1).
    spc : int, default 4
        Samples per chip (>= 1).
    chip_rate : float, default 1000000.0
        Chip rate in Hz (> 0).
    cn0_dbhz : float, default 50.0
        Carrier-to-noise density in dB-Hz (> 0).
    doppler_uncertainty : float, default 0.0
        One-sided Doppler search half-range in Hz.
    pfa : float, default 1e-3
        Target system false-alarm probability (0,1).
    pd : float, default 0.9
        Target detection probability (0,1).
    noise_mode : Literal["mean", "median", "min", "max"], default "mean"
        CFAR mode index: 0=mean, 1=median, 2=min, 3=max.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., reps: int = ..., spc: int = ..., chip_rate: float = ..., cn0_dbhz: float = ..., doppler_uncertainty: float = ..., pfa: float = ..., pd: float = ..., noise_mode: Literal["mean", "median", "min", "max"] = "mean") -> None: ...

    def reset(self) -> None:
        """Drain the input ring and reset the coherent accumulator. @param state Must be non-NULL.
        """

    def push(self, x: complex) -> list[tuple[int, int, float, float, float, float, int]]:
        """Stream raw samples; emit one event per CFAR dump above threshold. Forwards to acq_push() -- see its doc comment.

        Parameters
        ----------
        x : complex
            Input.

        Returns
        -------
        list[tuple[int, int, float, float, float, float, int]]
            Output.
        """

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the search grid directly, bypassing both auto-sizing searches -- the advanced escape hatch (mirrors Dll.configure_lock_raw/Costas.configure_lock). Resizes every buffer/plan that depends on the grid (the slow-time FFT, the code correlator, the reference, and every per-frame scratch buffer), re-derives the threshold ladder for the pinned grid from the same physics __init__ used, and clears in-flight accumulation (ring contents, the non-coherent power accumulator, dwell bookkeeping) -- call between push() calls, never a substitute for one. Raises ValueError if doppler_bins is outside [1, reps] or n_noncoh is outside [1, 256] (the internal non-coherent-look safety-valve ceiling).

        Parameters
        ----------
        doppler_bins : int
            Input.
        n_noncoh : int
            Input.
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
        """Coherent depth chosen: the slow-time FFT length in code reps (<= reps), unless doppler_uncertainty exceeds the native span, in which case this reports the wideband window-tile count instead (coherent depth forced to 1 -- see acq_core.h's file doc comment)."""

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
        """True when pd_predicted < pd -- the search cannot meet the target pd at this cn0_dbhz and geometry. The engine still builds a best-effort grid rather than failing; because C cannot raise a Python warning from a successful create, construction also emits a UserWarning in this case."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "BurstAcquisition": ...

    def __exit__(self, *args: object) -> None: ...

@final
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

@final
class BurstDemod:
    """Create a burst demodulator.

    Parameters
    ----------
    data_code : NDArray[np.uint8]
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

@final
class DsssReceiver:
    """Create a DSSS receiver in the searching state.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code, one 0/1 chip per element (0 -> +1, 1 -> -1 BPSK; only the low bit is used, so pass 0/1, not +/-1).
    chip_rate : float, default 1000000.0
        Chip rate, Hz. Required.
    symbol_rate : float, default 1000.0
        Data-symbol rate, Hz. Required — passed straight to the embedded Acquisition's own `symbol_rate` (diagnostic there; see `acq_create_continuous()`).
    spc : int, default 2
        Samples/chip (front-end oversample); default 2 (fs = 2x chip_rate).
    m : int, default 2
        PSK order, 2/4/8; default 2 (BPSK).
    cn0_dbhz : float, default 55.0
        Design C/N0 for acquisition sizing, dB-Hz; default 55.0.
    pfa : float, default 1e-3
        Acquisition false-alarm target; default 1e-3.
    pd : float, default 0.9
        Acquisition detection-probability target; default 0.9.
    doppler_uncertainty : float, default 100.0
        One-sided Doppler search half-range, Hz; default 100.0.
    segments : int, default 4
        Dll's own non-coherent partial-correlation count per code epoch — its tracking- robustness parameter, independent of `sps` (see the module docstring); default 4, this story's own validated sweet spot.
    sps : int, default 8
        MpskReceiver's samples/symbol, reached by an internal RateConverter bridging the despreader's own partial rate to this rate; default 8, MpskReceiver's own constructor default.
    differential : int, default 0
        MpskReceiver's differential (rotation- invariant) demap; default 0 (coherent).

    """
    def __init__(self, code: NDArray[np.uint8] = ..., chip_rate: float = ..., symbol_rate: float = ..., spc: int = ..., m: int = ..., cn0_dbhz: float = ..., pfa: float = ..., pd: float = ..., doppler_uncertainty: float = ..., segments: int = ..., sps: int = ..., differential: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Stream raw cf32 samples through the receiver. While searching, samples feed the embedded Acquisition and nothing is emitted (an empty array is normal, not an error). The moment a hit fires, Dll/RateConverter/MpskReceiver are built and seeded from it -- the same phase-inversion hand-off and rate-bridging this project's async-DSSS-receiver gallery story validated by hand -- and the unconsumed tail of this same call is handed straight to them, so no samples are dropped at the transition. While tracking, samples feed Dll -> RateConverter -> MpskReceiver in sequence and demodulated symbols are returned. Accepts any block size; state carries across calls.

        While searching, samples feed the embedded Acquisition and nothing is
        emitted (0 return is normal, not an error). The moment a hit fires,
        `Dll`/`RateConverter`/`MpskReceiver` are built and seeded from it, and
        the unconsumed tail of THIS call — computed exactly from
        `acq->samples_consumed`, no samples dropped or double-fed — is handed
        straight to them in the same call. While tracking, samples feed `Dll ->
        RateConverter -> MpskReceiver` in sequence. Accepts any block size;
        state carries across calls (`Acquisition`/`Dll`/
        `RateConverter`/`MpskReceiver` are all already block-size invariant, so
        this object needs no ring-buffering of its own).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written (0 while searching, or while tracking with not yet a full symbol's worth of input).
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded Acquisition's search grid directly, bypassing the symbol_rate-driven auto-sizing -- the escape hatch for a power user who wants a specific (doppler_bins, n_noncoh). Only meaningful while searching.

        Parameters
        ----------
        doppler_bins : int
            Input.
        n_noncoh : int
            Input.
        """

    def configure_lock_raw(self, up_thresh: float, down_thresh: float, n_looks: int, alpha: float, n_up: int, n_down: int) -> None:
        """Re-tune the embedded Dll's code-lock detector directly. Only meaningful once tracking has begun; a no-op while searching.

        Parameters
        ----------
        up_thresh : float
            Input.
        down_thresh : float
            Input.
        n_looks : int
            Input.
        alpha : float
            Input.
        n_up : int
            Input.
        n_down : int
            Input.
        """

    def configure_chain_raw(self, segments: int, sps: int, n: int) -> None:
        """Pin the despread/resample/demod grid directly, bypassing the create-time segments/sps defaults -- segments (Dll's tracking parameter) and sps/n (MpskReceiver's rate/carrier-arm parameters) stay independently overridable here, still bridged by a freshly-sized RateConverter, never coupled to each other. Only meaningful once tracking; rebuilds the chain with every replacement allocated first, so a failed pin leaves the receiver on its prior grid.

        The escape hatch for the one composition-specific knob this object adds
        beyond its children's own: `segments` (Dll's tracking parameter) and
        `sps`/`n` (MpskReceiver's sample-rate/carrier-arm parameters) are
        indepen­dently overridable here, still bridged by a freshly-sized
        `RateConverter` — never coupled to each other (see the module
        docstring). Rebuilds `dll`/`rc`/`rx` with every replacement allocated
        first, only freeing and adopting the old ones once every allocation has
        succeeded (mirrors `Acquisition`'s own `_regrid()` discipline) — a
        failed pin leaves the receiver tracking on its prior grid, not
        half-destroyed. Only meaningful once tracking (the grid defaults still
        apply to create-time auto-sizing for the next hit while searching; call
        `dsss_receiver_create()` with different `segments`/`sps` for that, or
        re-pin here again after the next hit).

        Parameters
        ----------
        segments : int
            Dll tracking segments per code period.
        sps : int
            MpskReceiver samples per symbol (the resample target).
        n : int
            MpskReceiver's carrier-arm count; must divide sps.
        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded Acquisition and frees Dll/RateConverter/MpskReceiver (rebuilt from scratch on the next hit).
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def tracking(self) -> int:
        """Tracking."""

    @property
    def doppler_hz(self) -> float:
        """Doppler hz."""

    @property
    def cn0_dbhz_est(self) -> float:
        """Cn0 dbhz est."""

    @property
    def segments(self) -> int:
        """Segments."""

    @property
    def sps(self) -> int:
        """Sps."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def chip_phase(self) -> float:
        """Dll's live tracked code phase (chips); 0.0 while searching."""

    @property
    def code_rate(self) -> float:
        """Dll's own tracking-quality indicator; 1.0 while searching."""

    @property
    def lock(self) -> float:
        """MpskReceiver's carrier lock EMA; 0.0 while searching."""

    @property
    def norm_freq(self) -> float:
        """MpskReceiver's tracked carrier frequency; 0.0 while searching."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "DsssReceiver": ...

    def __exit__(self, *args: object) -> None: ...

@final
class AsyncDsssReceiver:
    """Create an AsyncDsssReceiver in the searching state.

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code, one 0/1 chip per element (0 -> +1, 1 -> -1 BPSK; only the low bit is used, so pass 0/1, not +/-1).
    chip_rate : float, default 1000000.0
        Chip rate, Hz. Required.
    symbol_rate : float, default 1000.0
        Data-symbol rate, Hz. Required.
    spc : int, default 2
        Samples/chip; default 2.
    m : int, default 2
        PSK order, 2/4/8; default 2 (BPSK).
    cn0_dbhz : float, default 55.0
        Design C/N0, dB-Hz; default 55.0 -- feeds BOTH the embedded Acquisition's own sizing AND (derated by `refine_design_margin_db`) CarrierAcquisition's `design_snr`.
    pfa : float, default 1e-3
        Acquisition false-alarm target; default 1e-3. Also CarrierAcquisition's own `pfa`.
    pd : float, default 0.9
        Acquisition detection-probability target; default 0.9. Also CarrierAcquisition's own `pd`.
    doppler_uncertainty : float, default 100.0
        One-sided Doppler search half-range, Hz; default 100.0.
    segments : int, default 4
        Live-tracking Dll's own segments; default 4.
    sps : int, default 8
        MpskReceiver's samples/symbol; default 8.
    differential : int, default 0
        MpskReceiver's differential demap; default 0 (coherent).
    refine_max_error_db : float, default 0.5
        Max tolerable async-lookback correlation-power loss driving the refine-stage collection Dll's coherent-I&D window count via dll_lookback_segments(). Oversampling the epoch is required for the asynchronous data: the residual carrier rides a ~symbol_rate-wide data-modulated spectrum, so segments>1 (default yields 11 at tsamps=2046) samples it above Nyquist; segments=1 undersamples and aliases it. Default 0.5.
    refine_samples_per_symbol : int, default 4
        CarrierAcquisition's own operating rate = this * symbol_rate; default 4.
    refine_design_margin_db : float, default 14.0
        Empirical derating of cn0_dbhz before CarrierAcquisition's design_snr; default 14.0.
    refine_n_fft : int, default 64
        CarrierAcquisition's own block size; default 64.
    refine_zero_pad : int, default 8
        CarrierAcquisition's own zero_pad; default 8.
    refine_sequential : bool, default false
        CarrierAcquisition's own sequential mode; default false -- sequential mode's early per-block test fires on far too little averaging at SPEC's own Es/N0 floor (confirmed: as few as 4 blocks, 150-200+ Hz off); false waits the full design_snr-derived dwell_target, matching freq_refine.refine_seed_ carrier_acq()'s own validated default.
    refine_max_n_blocks : int, default 100000
        CarrierAcquisition's own give-up cap in sequential mode; default 100000.
    carrier_freq_hz : float, default 0.0
        Nominal RF carrier frequency, Hz, enabling carrier->code aiding; 0.0 (default) = off. When > 0, the coupled code-rate Doppler (carrier_offset/carrier_freq) is fed to the tracking Dll via dll_set_rate_aid() so the code loop rides a dilated clock the discriminator alone can't pull in at low SNR. Set to the receiver's own downlink RF frequency for a physically-coupled Doppler capture.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., chip_rate: float = ..., symbol_rate: float = ..., spc: int = ..., m: int = ..., cn0_dbhz: float = ..., pfa: float = ..., pd: float = ..., doppler_uncertainty: float = ..., segments: int = ..., sps: int = ..., differential: int = ..., refine_max_error_db: float = ..., refine_samples_per_symbol: int = ..., refine_design_margin_db: float = ..., refine_n_fft: int = ..., refine_zero_pad: int = ..., refine_sequential: bool = ..., refine_max_n_blocks: int = ..., carrier_freq_hz: float = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Stream raw cf32 samples through the receiver. While searching, samples feed the embedded Acquisition and nothing is emitted. On a hit, the refine stage (a frozen-carrier Dll collection feeding CarrierAcquisition) is built and seeded from it, and the unconsumed tail of this call is handed straight to it -- no samples dropped. Once CarrierAcquisition reports ready (or its own give-up cap is reached), the live tracking chain (Dll + per-partial Costas + RateConverter + MpskReceiver) is built fresh, seeded from the ORIGINAL handoff chip phase and the refined-or-unrefined Doppler estimate, and demodulated symbols are returned from then on. Accepts any block size; state carries across calls.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written (0 while searching/refining, or while tracking with not yet a full symbol's worth of input).
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def configure_search_raw(self, doppler_bins: int, n_noncoh: int) -> None:
        """Pin the embedded Acquisition's search grid directly, bypassing the symbol_rate-driven auto-sizing. Only meaningful while searching.

        Parameters
        ----------
        doppler_bins : int
            Input.
        n_noncoh : int
            Input.
        """

    def configure_lock_raw(self, up_thresh: float, down_thresh: float, n_looks: int, alpha: float, n_up: int, n_down: int) -> None:
        """Re-tune the live-tracking Dll's code-lock detector directly. Only meaningful once tracking has begun; a no-op while searching or refining.

        Parameters
        ----------
        up_thresh : float
            Input.
        down_thresh : float
            Input.
        n_looks : int
            Input.
        alpha : float
            Input.
        n_up : int
            Input.
        n_down : int
            Input.
        """

    def configure_chain_raw(self, segments: int, sps: int, n: int) -> None:
        """Pin the live-tracking despread/resample/demod grid directly, bypassing the create-time segments/sps defaults. Only meaningful once tracking; rebuilds the chain with every replacement allocated first, so a failed pin leaves the receiver on its prior grid.

        Parameters
        ----------
        segments : int
            Input.
        sps : int
            Input.
        n : int
            Input.
        """

    def reset(self) -> None:
        """Return to the searching state: resets the embedded Acquisition and frees every refine-stage/track-stage child (rebuilt from scratch on the next hit).
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def tracking(self) -> int:
        """1 once the live tracking chain is built and demodulating; 0 while searching or refining."""

    @property
    def refining(self) -> int:
        """1 while the refine stage (CarrierAcquisition collection) is active; 0 while searching or tracking."""

    @property
    def doppler_hz(self) -> float:
        """The current best Doppler estimate: the coarse handoff value while refining, the CarrierAcquisition-refined value once tracking."""

    @property
    def cn0_dbhz_est(self) -> float:
        """Cn0 dbhz est."""

    @property
    def segments(self) -> int:
        """Segments."""

    @property
    def sps(self) -> int:
        """Sps."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def chip_phase(self) -> float:
        """Chip phase."""

    @property
    def code_rate(self) -> float:
        """Code rate."""

    @property
    def lock(self) -> float:
        """Lock."""

    @property
    def norm_freq(self) -> float:
        """Smoothed carrier estimate (integrator only, cycles/sample of the MpskReceiver output rate); lags a Doppler ramp by the constant Type-II ramp error."""

    @property
    def nco_freq(self) -> float:
        """Live carrier loop-filter output = NCO frequency command (cycles/sample of the MpskReceiver output rate): its mean tracks a Doppler ramp with no lag, its variance is the carrier loop stress."""

    @property
    def locked(self) -> int:
        """Binary receiver lock: the hysteretic (up/down verify-counted) lock detector on the emitted symbols -- declared when lock_metric stays >= lock_threshold for the up-count and dropped below it for the down-count."""

    @property
    def lock_metric(self) -> float:
        """Symbol-lock metric: SNR-weighted running mean of the BPSK lock signal (I^2-Q^2)/(I^2+Q^2) = cos(2*phi) over the emitted symbols (locked -> ~+1). Drives `locked`; exposed for engineering debug."""

    @property
    def lock_threshold(self) -> float:
        """The lock_metric declare threshold `locked` latches above (the lockdet up_thresh); exposed alongside lock_metric for engineering debug."""

    @property
    def car_last_error(self) -> float:
        """Pre-despread Costas phase discriminator (rad): the residual carrier phase loop 1 (de-rotates before the Dll) is not nulling. Engineering debug."""

    @property
    def car_nco_freq(self) -> float:
        """Loop 1 (pre-despread Costas) loop-filter output = NCO frequency command, cycles/sample of the front-end (chip_rate*spc) rate. Engineering debug."""

    @property
    def mpsk_last_error(self) -> float:
        """MpskReceiver carrier phase discriminator (rad): the residual carrier phase loop 2 (post-despread) is not nulling. Engineering debug."""

    @property
    def code_locked(self) -> int:
        """Binary code-lock flag from the live tracking Dll's own verify-counted (pfa-tuned) lock detector -- the fundamental DSSS "am I despreading" lock, de-chattered by up/down hysteresis."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "AsyncDsssReceiver": ...

    def __exit__(self, *args: object) -> None: ...
