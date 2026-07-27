# track/track.pyi — type stubs for the track C extension.
from typing import final, Literal
import numpy as np
from numpy.typing import NDArray

@final
class LoopFilter:
    """LoopFilter component.

    Parameters
    ----------
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    t : float, default 1.0
        t constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import LoopFilter
    >>> obj = LoopFilter(bn=0.01, zeta=0.707, t=1.0)

    """
    def __init__(self, bn: float = ..., zeta: float = ..., t: float = ...) -> None: ...

    def step(self, x: float) -> float:
        """Advance the loop one update with error x; return the control.

        `integ += ki*x; return integ + kp*x`.

        Parameters
        ----------
        x : float
            Loop error.

        Returns
        -------
        float
            Control value (integ + kp*x).
        """

    def steps(self, x: NDArray[np.float64], out: NDArray[np.float64] | None = None) -> NDArray[np.float64]:
        """Run a block of errors through the loop.

        Parameters
        ----------
        x : NDArray[np.float64]
            Input.

        Returns
        -------
        NDArray[np.float64]
            Output.
        """

    def configure(self, bn: float, zeta: float, t: float) -> None:
        """Recompute the loop gains for a new (bn, zeta, t); preserves the integrator.

        Parameters
        ----------
        bn : float
            Loop noise bandwidth, normalized cycles/sample (>= 0).
        zeta : float
            Damping factor (typically 0.707).
        t : float
            Update period in samples (> 0).
        """

    def reset(self) -> None:
        """Zero the integrator; keep the configured gains.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def kp(self) -> float:
        """Kp."""

    @property
    def ki(self) -> float:
        """Ki."""

    @property
    def integ(self) -> float:
        """Integ."""
    @integ.setter
    def integ(self, value: float) -> None: ...

    @property
    def bn(self) -> float:
        """Bn."""

    @property
    def zeta(self) -> float:
        """Zeta."""

    @property
    def t(self) -> float:
        """T."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LoopFilter": ...

    def __exit__(self, *args: object) -> None: ...

@final
class Costas:
    """Costas component.

    Parameters
    ----------
    bn : float, default 0.05
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    init_norm_freq : float, default 0.0
        init_norm_freq constructor parameter.
    tsamps : int, default 64
        tsamps constructor parameter.
    bn_fll : float, default 0.0
        bn_fll constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import Costas
    >>> obj = Costas(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=64, bn_fll=0.0)

    """
    def __init__(self, bn: float = ..., zeta: float = ..., init_norm_freq: float = ..., tsamps: int = ..., bn_fll: float = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier, coherently integrate over each tsamps-sample symbol, run the decision-directed Costas discriminator, and emit one complex prompt symbol per symbol.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context and register the carrier loop's probes on it. Registers four probes, emitted once per dumped symbol and further thinned by decim: "<prefix>.lock" (the |Re P|/|P| lock-metric EMA, 1 = phase-locked), "<prefix>.e" (the PLL discriminator output — the loop stress), "<prefix>.freq" (the tracked NCO frequency, cycles/sample) and "<prefix>.locked" (the verify-counted lock decision, 0/1 — see costas_configure_lock). Passing NULL detaches.  Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in telemetry/telemetry.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "car" or "ch0.car".
        decim : int
            Emit every decim-th symbol; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Costas
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
        >>> c.set_telemetry(tlm, "car")
        >>> sorted(tlm.probe_names())
        ['car.e', 'car.freq', 'car.lock', 'car.locked']
        >>> x = np.ones(64 * 100, dtype=np.complex64)
        >>> _ = c.steps(x)
        >>> recs = tlm.read()   # four records per dumped symbol
        >>> len(recs) == 4 * 100
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the frequency/phase estimate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def configure_lock(self, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Re-tune the carrier lock detector: locked flips up after n_up consecutive dumped symbols with the lock-metric EMA above up_thresh, and drops after n_down consecutive symbols below down_thresh (level + time hysteresis; see detection.LockDet). The defaults (0.85/0.78, 8 up / 32 down) derive from the metric's no-carrier statistics: |Re P|/|P| averages 2/pi (~0.64) under H0 with an EMA-smoothed std of ~0.07, so the declare threshold sits ~3 sigma above the no-carrier mean. A live lock survives the re-tune; the in-flight verify run restarts.

        The always-on lock decision steps a verify-counted detector
        (lockdet_core.h) on the |Re P|/|P| lock-metric EMA once per dumped
        symbol: `locked` flips up after n_up consecutive symbols with the metric
        above up_thresh and drops after n_down consecutive symbols below
        down_thresh. The defaults derive from the metric's own H0 statistics —
        with no carrier, |Re P|/|P| = |cos(theta)| for a uniform theta, whose
        mean is 2/pi (~0.637) and per-symbol std ~0.31; the COSTAS_LOCK_ALPHA =
        0.1 EMA reduces that to ~0.071, so the default declare threshold 0.85
        sits ~3 sigma above the no-carrier mean, with the drop threshold at 0.78
        for level hysteresis and 8-up/32-down verify counts for time hysteresis
        (declare fast, drop reluctantly — the EMA already correlates adjacent
        looks, so the counts guard against band-edge dwell rather than
        compounding i.i.d. probabilities). A live lock survives the re-tune; the
        in-flight verify run restarts.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the lock-metric EMA.
        down_thresh : float
            Drop threshold (<= up_thresh for level hysteresis).
        n_up : int
            Consecutive above-threshold symbols to declare; clamped to >= 1.
        n_down : int
            Consecutive below-threshold symbols to drop; clamped to >= 1.

        Examples
        --------
        >>> from doppler.track import Costas
        >>> c = Costas(bn=0.05, zeta=0.707, tsamps=64)
        >>> c.locked
        False
        >>> c.configure_lock(0.9, 0.8, 4, 16)   # tighter declare, faster drop

        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve config.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock_metric(self) -> float:
        """Lock metric."""

    @property
    def locked(self) -> bool:
        """Current carrier lock decision: True after the verify count of consecutive above-threshold symbols, False again after the drop count of consecutive below-threshold ones (see configure_lock)."""

    @property
    def last_error(self) -> float:
        """Last error."""

    @property
    def bn_fll(self) -> float:
        """Bn fll."""
    @bn_fll.setter
    def bn_fll(self, value: float) -> None: ...

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Costas": ...

    def __exit__(self, *args: object) -> None: ...

@final
class Dll:
    """Create a DLL instance (COPIES code).

    Parameters
    ----------
    code : NDArray[np.uint8]
        Spreading code (0/1 chips), one period; copied internally.
    sps : int, default 2
        Samples per chip (default 2).
    init_chip : float, default 0.0
        Seed code phase, chips (default 0.0).
    bn : float, default 0.01
        Loop noise bandwidth (default 0.01).
    zeta : float, default 0.707
        Damping factor (default 0.707).
    spacing : float, default 0.5
        Early/late tap offset, chips (default 0.5).
    segments : int, default 1
        Partial correlations per code epoch (default 1). 1 = a coherent full-epoch integrate-and-dump (one prompt/period). >1 splits each epoch into that many sub-epoch partials: it emits that many partial prompts/period and tracks the code non-coherently across them (robust to an asynchronous data-symbol clock). segments/epoch ~ samples/symbol at a downstream SymbolSync when the symbol rate is near the code rate, so choose >= 2 for symbol-timing recovery.

    """
    def __init__(self, code: NDArray[np.uint8] = ..., sps: int = ..., init_chip: float = ..., bn: float = ..., zeta: float = ..., spacing: float = ..., segments: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Correlate a cf32 block against the local code with early/prompt/late taps and steer the code NCO each code period on the non-coherent (sum|E|-sum|L|)/(sum|E|+sum|L|) discriminator. With segments=1 (default) this is a coherent full-epoch integrate-and-dump: one prompt symbol per period. With segments>1 each epoch is split into that many sub-epoch partial correlations: it emits that many partial prompts per period (a stream at ~segments samples/symbol when the symbol rate is near the code rate) and tracks the code non-coherently across the partials, which a data flip cannot collapse (robust to an asynchronous data-symbol clock). segments>1 is the streaming despreader: it removes the PN code and outputs samples. The non-coherent loop is carrier-blind, so it tracks with a residual carrier still on the input; carrier recovery (Costas) and symbol-timing recovery (SymbolSync) are downstream stages fed from the partial output. Returned blocks are safe to keep across calls (block-size invariant): a block whose array is still referenced is never overwritten by a later call (jm gh-437).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context and register the code loop's probes on it. Registers four probes, emitted once per code epoch (period) and further thinned by decim: "<prefix>.e" (the early-minus-late envelope discriminator — the loop stress), "<prefix>.rate" (the tracked code rate, chips advanced per nominal chip, ~1.0 at lock), "<prefix>.lock" (the CFAR lock statistic R; compare against the configured threshold) and "<prefix>.locked" (the verify-counted lock decision, 0/1 — the lockdet output, so a consumer sees where the declare/drop rule fired without re-deriving it from the statistic).  Passing NULL detaches. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in telemetry/telemetry.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "code" or "ch0.code".
        decim : int
            Emit every decim-th epoch; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> code = np.zeros(31, dtype=np.uint8)
        >>> d = Dll(code=code, sps=2)
        >>> d.set_telemetry(tlm, "code")
        >>> sorted(tlm.probe_names())
        ['code.e', 'code.lock', 'code.locked', 'code.rate']
        >>> x = np.ones(31 * 2 * 50, dtype=np.complex64)
        >>> _ = d.steps(x)
        >>> recs = tlm.read()   # four records per code epoch
        >>> len(recs) > 0 and len(recs) % 4 == 0
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the code phase/rate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def set_rate_aid(self, rate_aid: float) -> None:
        """Set the carrier-aiding code-rate deviation (ratio; 0 = off): a fixed fractional rate bias summed into the code NCO's phase_inc every epoch, on top of the loop's own control. For physically-coupled Doppler, pass carrier_offset_hz / carrier_freq_hz so the code NCO rides the code-rate dilation the discriminator alone can't pull in at low SNR. Applied continuously across the epoch (not a phase pulse), and nudges the current phase_inc so the aid takes effect before the first period update. code_rate stays the loop's own observable and is unaffected.

        A fixed fractional rate bias summed into the sample-and-hold `phase_inc`
        on top of the loop's own control every epoch -- for physically-coupled
        Doppler, `carrier_offset_hz / carrier_freq_hz`, so the code NCO rides
        the code-rate dilation the discriminator alone can't pull in at low SNR.
        Applied continuously across the epoch (via `phase_inc`), not as a phase
        pulse. Also nudges the current `phase_inc` so the aid takes effect
        before the first period update. `code_rate` stays the loop's own
        observable and is unaffected.

        Parameters
        ----------
        rate_aid : float
            Fractional code-rate deviation (e.g. 8e-6). 0 disables.
        """

    def configure_lock(self, pfa: float, n_looks: int, ref_snr_db: float = 0.0) -> None:
        """Tune the always-on code-lock detector to a target (pfa, n_looks). The detector reuses acquisition's non-coherent statistic R = sqrt(2*sum|P|^2 / E|O|^2), where the prompt powers of n_looks consecutive looks are summed and E|O|^2 is an EMA of a random off-peak (noise) correlation re-drawn each epoch; a decision compares R against det_threshold_noncoherent(pfa, n_looks). Size n_looks with detection.det_n_noncoh(snr, ...) for your operating C/N0. The EMA bandwidth is sized probabilistically (detection.det_ema_alpha): ref_snr_db sets the noise reference's estimator SNR (mean^2/variance of the EMA output); the default 0.0 derives it from n_looks so the reference's std stays an eighth of the statistic's intrinsic H0 spread, floored at ~33 dB. Decisions feed a verify-counted lock detector rather than a single-comparison latch: locked flips up only after det_verify_count(pfa, pfa*1e-3) consecutive above-threshold decisions (2 for the default pfa=1e-3, compounding the false-declare rate three decades under pfa) and drops only after 2 consecutive below-threshold decisions, so a statistic grazing the threshold cannot chatter the flag. The default config is pfa=1e-3 over 20 looks. Raises ValueError for pfa outside (0, 1). Read the result from the locked / lock_stat / noise_est properties.

        The DLL carries a lock detector that reuses acquisition's non-coherent
        test statistic. Every emitted look (a partial in segments mode, or the
        full-epoch prompt when segments == 1) is also correlated at a *random
        off-peak* code phase — re-drawn each epoch and kept `noise_guard` chips
        clear of the prompt/early/late lobe — to give a signal-free CFAR noise
        sample (valid for a low-sidelobe code, e.g. Gold). The offset power
        feeds an EMA reference `E|O|^2`; the prompt powers of n_looks
        consecutive looks are summed into `S = sum|P_k|^2`, and the detector
        declares lock when

        R = sqrt(2 * S / E|O|^2) > det_threshold_noncoherent(pfa, n_looks)

        which under H0 has `P(R > eta) = marcum_q(n_looks, 0, eta)`. Size
        n_looks with det_n_noncoh(snr, ...) for the operating C/N0.

        The noise-reference EMA bandwidth is sized probabilistically via
        det_ema_alpha(): the signal-free `|O|^2` samples are exponential (0 dB
        estimator SNR per sample — a DC level in fluctuation of equal power),
        and ref_snr_db chooses the EMA output's estimator SNR (mean^2/variance).
        Passing 0 derives it from n_looks: the reference's relative std is held
        to an eighth of the statistic's intrinsic H0 spread (`1/sqrt(N)`),
        floored at ~33 dB — which reproduces the classic `1/alpha = max(1024,
        32*N)` sizing exactly, now as a consequence instead of a constant.

        The detector needs an off-peak code phase to sample noise from: with a
        very short code (fewer than ~2*(spacing+2)+1 chips, i.e. sf <= 6 at the
        default spacing) no offset clears the prompt/early/late lobe, the noise
        tap aliases the prompt, and the statistic pins below threshold — locked
        stays 0 (fail-closed) no matter the signal. Use a code of >= 7 chips
        (real spreading codes are far longer) for a meaningful lock decision.

        The decision itself runs through an embedded lock detector
        (lockdet_core.h) rather than a single-comparison latch: `locked` flips
        up only after det_verify_count(pfa, pfa*1e-3) CONSECUTIVE
        above-threshold decisions (the false-declare budget held three decades
        under the per-decision pfa — 2 straight for the default 1e-3), and drops
        only after 2 straight below-threshold decisions, so a statistic grazing
        the threshold cannot chatter the flag. Full control of the verify counts
        and a split declare/drop threshold pair is C-only via
        dll_configure_lock_raw().

        Parameters
        ----------
        pfa : float
            Per-decision false-alarm probability, in (0, 1).
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        ref_snr_db : float
            Noise-reference estimator SNR in dB (> 0), or 0 to derive from n_looks as above.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import Dll
        >>> d = Dll(code=np.zeros(31, dtype=np.uint8), sps=2)
        >>> d.configure_lock(1e-3, 20)
        >>> d.locked
        False
        >>> d.configure_lock(1e-3, 20, ref_snr_db=20.0)   # ~50-look reference
        >>> d.configure_lock(2.0, 20)
        Traceback (most recent call last):
            ...
        ValueError: configure_lock failed (rc=-4)

        """

    def configure_lock_raw(self, up_thresh: float, down_thresh: float, n_looks: int, alpha: float, n_up: int, n_down: int) -> None:
        """Escape hatch under configure_lock() for direct control of the lock detector's geometry: a split declare/drop threshold pair on the statistic R (level hysteresis), the noise-EMA coefficient alpha, and both verify counts n_up/n_down (time hysteresis) independently -- configure_lock() only ever derives a symmetric threshold (up_thresh == down_thresh) and a fixed n_down=2. Re-tuning clears the in-flight statistic and drops the lock so the next decision uses only looks gathered under the new config. Size up_thresh/down_thresh with detection.det_threshold_noncoherent(pfa, n_looks), alpha with detection.det_ema_alpha, and n_up/n_down with detection.det_verify_count. Read the result from the locked / lock_stat / noise_est properties.

        The escape hatch under dll_configure_lock() for a composing C caller
        that derives its own threshold/EMA/hysteresis geometry — the full
        lockdet decision rule is exposed: a split declare/drop threshold pair
        (level hysteresis) and both verify counts (time hysteresis; size them
        with det_verify_count()). Re-tuning clears the in-flight statistic and
        drops the lock so the next decision uses only looks gathered under the
        new config.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the statistic R (e.g. the CFAR eta from det_threshold_noncoherent()).
        down_thresh : float
            Drop threshold on R; choose <= up_thresh for level hysteresis.
        n_looks : int
            Non-coherent integration depth N (looks); clamped >= 1.
        alpha : float
            EMA coefficient for the noise reference, in (0, 1].
        n_up : int
            Consecutive above-threshold decisions to declare lock; clamped to >= 1.
        n_down : int
            Consecutive below-threshold decisions to drop it; clamped to >= 1.
        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time code phase; preserve config.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def code_phase(self) -> float:
        """Code phase."""

    @property
    def code_rate(self) -> float:
        """Code rate."""

    @property
    def last_error(self) -> float:
        """Last error."""

    @property
    def segments(self) -> int:
        """Segments."""

    @property
    def locked(self) -> bool:
        """Current lock decision: True after the verify count of consecutive above-threshold N-look decisions, False again after the drop count of consecutive below-threshold ones (see configure_lock)."""

    @property
    def lock_stat(self) -> float:
        """Last code-lock test statistic R = sqrt(2*sum|P|^2 / E|O|^2); compare against det_threshold_noncoherent(pfa, n_looks)."""

    @property
    def noise_est(self) -> float:
        """Current CFAR noise-power estimate E|O|^2 from the off-peak (noise) tap EMA."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Dll": ...

    def __exit__(self, *args: object) -> None: ...

@final
class SymbolSync:
    """SymbolSync component.

    Parameters
    ----------
    sps : int, default 4
        sps constructor parameter.
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    order : Literal["linear", "parabolic", "cubic"], default "cubic"
        order constructor parameter.
    ted : Literal["gardner", "dttl"], default "gardner"
        Timing-error detector: "gardner" (blind, works for any constellation) or "dttl" (decision-directed sign-sign Data Transition Tracking Loop; lower self-noise near lock but degrades faster at low SNR. BPSK/QPSK only -- invalid for 8PSK/QAM).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import SymbolSync
    >>> obj = SymbolSync(sps=4, bn=0.01, zeta=0.707, order="cubic", ted="gardner")

    """
    def __init__(self, sps: int = ..., bn: float = ..., zeta: float = ..., order: Literal["linear", "parabolic", "cubic"] = "cubic", ted: Literal["gardner", "dttl"] = "gardner") -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Recover symbol timing from an oversampled cf32 baseband block: a timing-error detector (Gardner or DTTL, see the `ted` param) drives an integer timing NCO whose post-wrap value gives the interpolation fraction for free, and a Farrow interpolator emits one symbol-rate sample per recovered symbol instant.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context and register the timing loop's probes on it. Registers five probes, emitted once per recovered symbol and further thinned by decim: "<prefix>.e" (the normalised TED error — the loop stress), "<prefix>.freq" (the loop-filter control steering the timing NCO, fractional rate offset), "<prefix>.rate" (the smoothed tracked samples/symbol), "<prefix>.lock" (the last block-averaged lock_signal, held between avgs-look updates) and "<prefix>.locked" (the verify-counted lockdet decision, 0/1). Passing NULL detaches.  Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in telemetry/telemetry.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "sync" or "rx.sync".
        decim : int
            Emit every decim-th symbol; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import SymbolSync
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 12)
        >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
        >>> ss.set_telemetry(tlm, "sync")
        >>> sorted(tlm.probe_names())
        ['sync.e', 'sync.freq', 'sync.lock', 'sync.locked', 'sync.rate']
        >>> x = np.repeat([1 + 1j, -1 - 1j], 4 * 64).astype(np.complex64)
        >>> _ = ss.steps(x)
        >>> recs = tlm.read()   # five records per recovered symbol
        >>> len(recs) > 0 and len(recs) % 5 == 0
        True

        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserve the timing estimate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def configure_lock(self, rolloff: float, esno_min_db: float, pfa: float, pd: float) -> None:
        """Tune the always-on timing-lock detector to a target (pfa, pd) at a given link operating point. The statistic is a Gardner-style eye-opening ratio, lock_signal = 2*(|on-time|^2-|mid-symbol|^2)/(|on-time|^2+|mid-symbol|^2), non-coherently block-averaged over avgs looks before each decision (mirroring Dll's tumbling-window CFAR pattern). avgs and the declare threshold are sized from a Gaussian approximation: a per-look mean is estimated from rolloff and esno_min_db, then the classic N = variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold = Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation gives (avgs, threshold). No level hysteresis by default (up=down=threshold, matching Dll.configure_lock's shape); n_up=1, n_down=8. Raises ValueError if pfa/pd are outside (0, 1) or pd does not exceed pfa. Read the result from the locked / lock_stat properties.

        Sizes the non-coherent block size (avgs) and declare threshold from a
        Gaussian sizing of the eye-opening statistic lock_signal =
        2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2): a per-look mean
        (mean_lock_detect, from rolloff and the minimum operating Es/N0) drives
        the classic N = variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 / threshold =
        Q^-1(pfa)*mean/(Q^-1(pfa)-Q^-1(pd)) derivation, implemented directly
        from a formula supplied by a doppler user (not re-derived against a
        primary source), with "variance" set from a direct measurement of
        lock_signal's real per-look variance under noise (~1.343,
        5,000,000-sample Monte Carlo) rather than the placeholder "8" this API
        originally shipped with -- see symsync_core.c's
        SYMSYNC_LOCK_STAT_VARIANCE comment for the full derivation (a
        factor-of-2 correction for the erfcinv-vs-Q^-1 convention applies on top
        of the measured variance; the two hypotheses were empirically compared
        before picking one). Empirically validated at the default operating
        point (avgs=133, threshold=0.311): 429 false declares over 500,000
        independent noise-only blocks against a nominal pfa=1e-3 (8.58e-4,
        correctly sized with safe margin, not accidentally oversized); 2000/2000
        true declares at the esno_min design SNR against a nominal pd=0.9 -- see
        native/validation/symsync_lock.c for the harness. No level hysteresis by
        default (up = down = threshold, matching dll_configure_lock's shape);
        the raw escape hatch (symsync_configure_lock_raw) exposes split
        thresholds, an explicit avgs, and independent n_up/n_down.

        Parameters
        ----------
        rolloff : float
            Matched-filter excess bandwidth (e.g. 0.35 for a typical RRC system).
        esno_min_db : float
            Minimum operating Es/N0, dB -- the worst-case link point the detector must still declare lock at.
        pfa : float
            Target false-alarm probability per decision, in (0, 1).
        pd : float
            Target detection probability per decision, in (0, 1); must exceed pfa.

        Examples
        --------
        >>> from doppler.track import SymbolSync
        >>> ss = SymbolSync(sps=4, bn=0.01, zeta=0.707)
        >>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=1e-3, pd=0.9)
        >>> ss.locked
        False
        >>> ss.configure_lock(rolloff=0.35, esno_min_db=10.0, pfa=0.9, pd=0.9)
        Traceback (most recent call last):
            ...
        ValueError: configure_lock failed (rc=-4)

        """

    def configure_lock_raw(self, avgs: int, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Escape hatch under configure_lock() for direct control of the lock detector's geometry: an explicit non-coherent block size (avgs), a split declare/drop threshold pair on lock_stat (level hysteresis), and both verify counts (time hysteresis) independently. Re-tuning clears the in-flight block sum and drops the lock so the next decision uses only looks gathered under the new config.

        The escape hatch under symsync_configure_lock() for a caller that
        derives its own averaging/threshold geometry: the block size (avgs), a
        split declare/drop threshold pair on lock_stat (level hysteresis), and
        both verify counts (time hysteresis). Re-tuning clears the in-flight
        block sum and drops the lock so the next decision uses only looks
        gathered under the new config.

        Parameters
        ----------
        avgs : int
            Non-coherent block size (looks/decision); clamped >= 1.
        up_thresh : float
            Declare threshold on lock_stat.
        down_thresh : float
            Drop threshold; choose <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold decisions to declare; clamped >= 1.
        n_down : int
            Consecutive below-threshold decisions to drop; clamped >= 1.
        """

    def reset(self) -> None:
        """Re-seed the timing loop to its nominal rate and zero phase.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def timing_error(self) -> float:
        """Timing error."""

    @property
    def rate(self) -> float:
        """Rate."""

    @property
    def lock_stat(self) -> float:
        """Last block-averaged lock statistic: mean(2*(|on-time|^2-|mid-symbol|^2)/(|on-time|^2+|mid-symbol|^2)) over the configured avgs looks; compare against the configured threshold (see configure_lock)."""

    @property
    def locked(self) -> bool:
        """Current timing-lock decision: True after the verify count of consecutive above-threshold decisions, False again after the drop count of consecutive below-threshold ones (see configure_lock)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "SymbolSync": ...

    def __exit__(self, *args: object) -> None: ...

@final
class RateSync:
    """RateSync component.

    Parameters
    ----------
    sps : float, default 4.0
        Nominal samples per symbol. Any double >= `m` -- 17.33389 is as valid as 4, because the terminal stage's accumulator is a double and the loop only has to steer the strobe. That is the real-world case whenever the ADC clock is free-running against the symbol clock.
    pulse : Literal["iandd", "rrc"], default "rrc"
        Matched-filter pulse shape: "rrc" (root-raised cosine, roll-off `beta`) or "iandd" (unit rectangle one symbol wide -- the matched filter for a rectangular symbol, and exactly what an integrate-and-dump computes). The rectangle needs far fewer taps, so an NRZ link's matched filter is cheaper.
    beta : float, default 0.35
        beta constructor parameter.
    span : int, default 8
        span constructor parameter.
    m : int, default 2
        Terminal outputs per symbol: even, 2 <= m <= 8. Gardner needs a transition gate half a symbol from the on-time strobe, which is why m must be even and at least 2. The oversampled stream is a by-product of the same dot products, not an extra cost. Use m >= 4 with pulse="iandd": the rectangle is one symbol wide, so at m=2 its matched filter is a 2-tap sum and the eye statistic barely opens (measured lock_stat -0.34 at m=2 against +0.95 at m=4 on the same NRZ stream). The RRC spans many symbols and is unaffected.
    num_phases : int, default 1024
        Matched-filter arms; a power of two. Sets the fractional-timing resolution to 1/num_phases of an output period.
    bn : float, default 0.01
        bn constructor parameter.
    zeta : float, default 0.707
        zeta constructor parameter.
    ted : Literal["gardner", "dttl"], default "gardner"
        Timing-error detector: "gardner" (blind, works for any constellation) or "dttl" (decision-directed sign-sign Data Transition Tracking Loop; lower self-noise near lock but degrades faster at low SNR. BPSK/QPSK only -- invalid for 8PSK/QAM).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import RateSync
    >>> obj = RateSync(sps=4.0, pulse="rrc", beta=0.35, span=8, m=2, num_phases=1024, bn=0.01, zeta=0.707, ted="gardner")

    """
    def __init__(self, sps: float = ..., pulse: Literal["iandd", "rrc"] = "rrc", beta: float = ..., span: int = ..., m: int = ..., num_phases: int = ..., bn: float = ..., zeta: float = ..., ted: Literal["gardner", "dttl"] = "gardner") -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Recover symbols from an oversampled cf32 baseband block. The owned RateConverter's terminal stage IS the matched filter, and the polyphase arm its accumulator selects IS the fractional timing delay, so one dot product does the rate conversion, the matched filtering and the interpolation. Every m-th output is an on-time strobe and the output m/2 back is the transition gate; a Gardner or DTTL detector drives a PI loop that steers the terminal stage's control port. State carries across calls, so contiguous blocks give the same symbols as one large block.

        ratesync_step() in a loop, with the TED specialised per detector; state
        carries across calls, so contiguous blocks give the same symbols as one
        large block.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input samples.

        Returns
        -------
        NDArray[np.complex64]
            Symbols written to out.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context and register the probes.

        Registers six probes, emitted once per recovered symbol and further
        thinned by decim: "<prefix>.e" (normalised TED error), "<prefix>.ctrl"
        (the per-input control steering the strobe), "<prefix>.rate" (tracked
        samples/symbol), "<prefix>.lock" (last block-averaged lock_signal),
        "<prefix>.locked" (0/1) and "<prefix>.mu" (the timing NCO's fractional
        phase — see resamp_get_ctrl_acc()). Passing NULL detaches. Setup path,
        never hot: the context is borrowed and must outlive the attachment (SPSC
        rules in telemetry/telemetry.h).

        The three form one readable picture of the loop: `e` is what the
        detector saw, `ctrl` is what the filter did about it, and `mu` is where
        the sampling instant ended up as a result — the only one of the three
        that is a physical position rather than a correction.

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "sync".
        decim : int
            Emit every decim-th symbol; >= 1.
        """

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserve the timing estimate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def configure_lock_raw(self, avgs: int, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Direct control of the lock detector's geometry: an explicit non-coherent block size (avgs), a split declare/drop threshold pair on lock_stat (level hysteresis), and both verify counts (time hysteresis) independently. Re-tuning clears the in-flight block sum and drops the lock so the next decision uses only looks gathered under the new config. The (pfa, pd) sizing entry point symsync exposes is deliberately not mirrored here: its constants were calibrated against symsync's own geometry by Monte Carlo, and re-exposing the formula for a different front end without repeating that validation would assert a calibration nobody measured.

        The block size (avgs), a split declare/drop threshold pair on lock_stat
        (level hysteresis) and both verify counts (time hysteresis). Re-tuning
        clears the in-flight block sum and drops the lock, so the next decision
        uses only looks gathered under the new config.

        Parameters
        ----------
        avgs : int
            Looks per decision; clamped >= 1.
        up_thresh : float
            Declare threshold on lock_stat.
        down_thresh : float
            Drop threshold; <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold decisions to declare.
        n_down : int
            Consecutive below-threshold decisions to drop.
        """

    def reset(self) -> None:
        """Re-seed the timing loop, the cascade's filter memories, the strobe ring and the prime countdown.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def timing_error(self) -> float:
        """Last normalised TED error — the loop stress."""

    @property
    def rate(self) -> float:
        """Smoothed tracked samples per symbol. Departs from the nominal `sps` by exactly the sample-clock offset being tracked, so it is the estimator a rate-disciplining caller reads."""

    @property
    def ctrl(self) -> float:
        """Current per-input rate deviation steering the terminal stage's accumulator."""

    @property
    def lock_stat(self) -> float:
        """Last block-averaged lock statistic: mean(2*(|on-time|^2-|mid|^2)/(|on-time|^2+|mid|^2)) over the configured avgs looks. This, not an error-vector magnitude, is the honest lock indicator -- a single cycle slip during acquisition drags a windowed EVM by 20 dB while the eye stays wide open at +0.75."""

    @property
    def locked(self) -> bool:
        """Current timing-lock decision: True after the verify count of consecutive above-threshold decisions, False again after the drop count of consecutive below-threshold ones."""

    @property
    def clipped(self) -> bool:
        """True if the cascade's CIC stage has clipped its input since the last reset(). A CIC bounds its input to +-1.0 and clips silently past that, which no timing metric reveals -- an overdriven front end degrades EVM by 25 dB with a perfectly healthy lock. Always False when the plan contains no CIC stage."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "RateSync": ...

    def __exit__(self, *args: object) -> None: ...

@final
class CarrierMpsk:
    """Create an M-PSK carrier loop instance.

    Parameters
    ----------
    bn : float, default 0.05
        Loop noise bandwidth (default 0.05).
    zeta : float, default 0.707
        Damping factor (default 0.707).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample (default 0.0).
    tsamps : int, default 64
        Samples per symbol (default 64).
    bn_fll : float, default 0.0
        FLL-assist bandwidth (default 0.0 = pure PLL).
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import CarrierMpsk
    >>> obj = CarrierMpsk(bn=0.05, zeta=0.707, init_norm_freq=0.0, tsamps=64, bn_fll=0.0, m=4)

    """
    def __init__(self, bn: float = ..., zeta: float = ..., init_norm_freq: float = ..., tsamps: int = ..., bn_fll: float = ..., m: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier, coherently integrate over each tsamps-sample symbol, run the decision-directed M-PSK discriminator (slice to the nearest constellation point, error Im(P*conj(ahat))/|P|), and emit one complex prompt symbol per symbol. The loop tracks a small residual carrier (bulk Doppler removed upstream); it locks to one of m phases, so resolve the M-fold ambiguity downstream (mpsk_diff_demap or a sync word). At m=2 this is exactly the BPSK Costas loop.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def configure(self, bn: float, zeta: float) -> None:
        """Recompute the loop gains for a new (bn, zeta); preserves the frequency/phase estimate.

        Parameters
        ----------
        bn : float
            Input.
        zeta : float
            Input.
        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve config.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def norm_freq(self) -> float:
        """Norm freq."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock_metric(self) -> float:
        """Lock metric."""

    @property
    def last_error(self) -> float:
        """Last error."""

    @property
    def bn_fll(self) -> float:
        """Bn fll."""
    @bn_fll.setter
    def bn_fll(self, value: float) -> None: ...

    @property
    def m(self) -> int:
        """M."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CarrierMpsk": ...

    def __exit__(self, *args: object) -> None: ...

@final
class CarrierNda:
    """Create an NDA carrier loop instance.

    Parameters
    ----------
    bn : float, default 0.01
        Loop noise bandwidth (default 0.01).
    zeta : float, default 0.707
        Damping factor (default 0.707).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample (default 0.0).
    sps : int, default 8
        Samples per symbol (default 8).
    n : int, default 4
        MA window divisor: window = sps/n (default 4; sps%n==0).
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import CarrierNda
    >>> obj = CarrierNda(bn=0.01, zeta=0.707, init_norm_freq=0.0, sps=8, n=4, m=4)

    """
    def __init__(self, bn: float = ..., zeta: float = ..., init_norm_freq: float = ..., sps: int = ..., n: int = ..., m: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """De-rotate a cf32 block with the integer-NCO carrier and return the de-rotated samples (one per input sample). Internally the loop runs a non-data-aided M-th-power discriminator on an I/Q arm integrate-and-dump at n dumps per symbol and steers the NCO, so it acquires the carrier with no symbol timing and no data present (it strips the M-PSK modulation by raising the arm sample to the Mth power). It locks to one of m phases (M-fold ambiguity), resolved downstream. Read norm_freq for the tracked carrier and lock for the carrier lock metric.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context and register the carrier loop's probes on it — including the embedded arm AGC's. Registers four probes of its own, emitted once per input sample (this is a sample-rate loop — use decim to thin the stream) plus the embedded AGC's "<prefix>.agc.gain_db" (emitted at the AGC's own amortized gain-update rate): "<prefix>.lock" (the lock-signal EMA, ~1 when phase-locked), "<prefix>.e" (the M-th-power phase discriminator — the loop stress), "<prefix>.freq" (the tracked carrier frequency, cycles/sample) and "<prefix>.locked" (the verify-counted lockdet decision, 0/1).  Passing NULL detaches the loop and the embedded AGC. Setup path, never hot: call before the producer thread starts stepping; the context is borrowed and must outlive the attachment (SPSC rules in telemetry/telemetry.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "car" or "rx.car".
        decim : int
            Emit every decim-th sample; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import CarrierNda
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)
        >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
        >>> c.set_telemetry(tlm, "car", decim=8)
        >>> sorted(tlm.probe_names())
        ['car.agc.gain_db', 'car.e', 'car.freq', 'car.lock', 'car.locked']
        >>> x = np.exp(2j * np.pi * 0.005 * np.arange(4096)).astype(np.complex64)
        >>> _ = c.steps(x)
        >>> recs = tlm.read()
        >>> len(recs[recs["probe"] == tlm.probe_id("car.e")]) == 4096 // 8
        True

        """

    def configure_lock(self, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Re-tune the carrier lock detector: locked flips up after n_up consecutive samples with the lock-signal EMA above up_thresh, and drops after n_down consecutive samples below down_thresh (level + time hysteresis; see detection.LockDet). Defaults (0.5/0.4, 8 up / 32 down) mirror MpskReceiver's own pre-existing acquisition<->tracking handover, which already steps a lockdet on this exact statistic and is validated by that receiver's BER regression gate. A live lock survives the re-tune; the in-flight verify run restarts.

        Full lockdet control, mirroring costas_configure_lock(): a split
        declare/drop threshold pair on the lock-signal EMA (level hysteresis)
        and both verify counts (time hysteresis). Defaults (0.5/0.4, 64 up / 32
        down) start from MpskReceiver's own pre-existing acquisition<-> tracking
        handover thresholds, but size n_up independently: `lock` is a fast
        per-sample EMA, so consecutive looks are highly autocorrelated and
        MpskReceiver's own n_up=8 does not compound the false-declare rate the
        way it would for independent looks (direct Monte Carlo against a
        noise-only, no-carrier input found real false locks at n_up=8; n_up=64
        was the smallest verify count that reliably eliminated them -- see
        carrier_nda_core.c's CARRIER_NDA_LOCK_DEFAULT_* comment for the exact
        trial data). A live lock survives the re-tune; the in-flight verify run
        restarts.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the lock-signal EMA.
        down_thresh : float
            Drop threshold; choose <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold samples to declare; clamped >= 1.
        n_down : int
            Consecutive below-threshold samples to drop; clamped >= 1.

        Examples
        --------
        >>> from doppler.track import CarrierNda
        >>> c = CarrierNda(bn=0.01, sps=8, n=4, m=4)
        >>> c.locked
        False
        >>> c.configure_lock(0.6, 0.5, 16, 64)   # tighter declare, slower drop

        """

    def reset(self) -> None:
        """Re-seed the loop to the create-time frequency/phase; preserve config.
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
    def lock(self) -> float:
        """Lock."""

    @property
    def locked(self) -> bool:
        """Current lock decision: True after the verify count of consecutive above-threshold samples, False again after the drop count of consecutive below-threshold ones (see configure_lock)."""

    @property
    def last_error(self) -> float:
        """Last error."""

    @property
    def bn(self) -> float:
        """Bn."""
    @bn.setter
    def bn(self, value: float) -> None: ...

    @property
    def m(self) -> int:
        """M."""

    @property
    def n(self) -> int:
        """N."""

    @property
    def sps(self) -> int:
        """Sps."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "CarrierNda": ...

    def __exit__(self, *args: object) -> None: ...

@final
class MpskReceiver:
    """Create an M-PSK receiver.

    Parameters
    ----------
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).
    sps : float, default 8.0
        Samples per symbol. Any double >= `m_out` -- 17.33389 is as valid as 8, because the front end plans its own cascade and the terminal stage's accumulator is a double. That is the real-world case whenever the ADC clock is free-running against the symbol clock.
    m_out : int, default 8
        Terminal outputs per symbol: even, 2..8. The Gardner detector takes every m_out-th output as the on-time strobe and the one m_out/2 back as the transition gate, so the oversampled matched-filtered stream falls out for free. **The default is 8 because that is where the I&D matched filter reaches the coherent bound.** The rectangle is one symbol wide, so its matched filter is an m_out-tap sum spanning it, and a smaller m_out samples that same integral more coarsely. Measured on QPSK at the default sps=8 against the coherent bound EVM_dB = -(Es/N0)_dB: at 18 dB Es/N0, m_out=8 lands 0.41 dB off the bound where m_out=4 loses 3.11 dB; at 14 dB it is 0.25 dB against 1.71 dB -- the gap widens as noise stops hiding it. **Never pair 2 with pulse="iandd"**: the matched filter degenerates to a two-tap sum, the eye barely opens (measured lock statistic -0.34 at 2 against +0.95 at 4 on the same NRZ stream) and acquisition itself fails about half the time (4/8 seeds locked at 14 dB Es/N0, against 8/8 at both 4 and 8). Replaces the old `n` (NDA arm dumps per symbol): the cascade's own outputs now feed the carrier discriminator, so there is no separate arm to size.
    pulse : Literal["iandd", "rrc"], default "iandd"
        Matched-filter shape (default MPSK_RX_PULSE_IANDD).
    rrc_beta : float, default 0.35
        RRC roll-off in `[0, 1]` (default 0.35; RRC only).
    rrc_span : int, default 8
        RRC one-sided span in symbols (default 8; RRC only).
    bn_carrier : float, default 0.01
        Carrier loop noise bandwidth, **normalised to the symbol rate** (default 0.01). A carrier loop here closes around the matched filter, so its dead time is that filter's group delay — keep it a small fraction of the symbol rate, as a real receiver does.
    zeta : float, default 0.707
        Damping factor for both loops (default 0.707).
    bn_timing : float, default 0.01
        Symbol-timing loop noise bandwidth, normalised to the symbol rate (default 0.01).
    acq_to_track : int, default 0
        Enable the two-way NDA<->decision-directed handover (default 0).
    lock_thresh : float, default 0.5
        Handover declare threshold on the carrier lock metric (default 0.5); the drop threshold sits at 0.8x for level hysteresis, and both directions are verify-counted (8 symbols up / 32 down). The metric is `Re((z/|z|)^M)` smoothed by an EMA, whose noise-only sd is 0.1132 for **every** M, so the threshold is `0.5 / 0.1132` = 4.42 noise sigmas — a per-look false-alarm probability of 5e-6. Pick a value by dividing your Pfa's z-score into 0.1132 rather than by feel; see carrier_nda_core.h for the derivation and the measured verification.
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample at the input rate (default 0.0). This is the centre the LO is tuned to; the loop tracks the residual around it.
    warmup_syms : int, default 100
        Symbols before the acq-to-track switch is allowed (default 100).
    differential : int, default 0
        bits(): differential (rotation-invariant) demap (default 0 = coherent).
    num_phases : int, default 1024
        Matched-filter bank arms; a power of two. Sets the fractional-timing resolution to 1/num_phases of an output period. The bank is sized by the POST-decimation rate, so this costs the same at sps=8 and sps=256.
    nda_tap : Literal["strobe", "mf_all", "lo_arm"], default "strobe"
        Where the NDA carrier discriminator reads from, which sets its pull-in range and whether it needs symbol timing at all. An M-th-power detector updating at rate F can only see |df| < F/(2M), so the tap point IS the range. `strobe` (default) reads the on-time strobe at the symbol rate Rs: the cleanest input, the narrowest range (Rs/(2M)), and the only tap whose input quality depends on the timing loop -- it steers from its first strobe whether or not timing has declared, so when the carrier must acquire before timing does, that is a reason to pick another tap rather than something the receiver resolves for you. `mf_all` reads every terminal output at m_out*Rs -- m_out times the range and no timing dependence, paid for with the ISI the between-symbol outputs carry, which hurts most where the decision margin is smallest (8PSK). `lo_arm` reads ahead of the cascade through a free-running half-symbol boxcar at the LO rate -- the widest range and fully timing-independent, but unmatched, so it pays squaring loss. Fixed at construction: nothing switches underneath you. If you need more range than any tap gives, put a coarse frequency estimate in front and pass it as init_norm_freq.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import MpskReceiver
    >>> obj = MpskReceiver(m=4, sps=8.0, m_out=8, pulse="iandd", rrc_beta=0.35, rrc_span=8, bn_carrier=0.01, zeta=0.707, bn_timing=0.01, acq_to_track=0, lock_thresh=0.5, init_norm_freq=0.0, warmup_syms=100, differential=0, num_phases=1024, nda_tap="strobe")

    """
    def __init__(self, m: int = ..., sps: float = ..., m_out: int = ..., pulse: Literal["iandd", "rrc"] = "iandd", rrc_beta: float = ..., rrc_span: int = ..., bn_carrier: float = ..., zeta: float = ..., bn_timing: float = ..., acq_to_track: int = ..., lock_thresh: float = ..., init_norm_freq: float = ..., warmup_syms: int = ..., differential: int = ..., num_phases: int = ..., nda_tap: Literal["strobe", "mf_all", "lo_arm"] = "strobe") -> None: ...

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) a telemetry context across the receiver. Registers the receiver's own "<prefix>.lock" probe (the carrier lock EMA) and "<prefix>.tracking" (the two-way handover decision, 0/1 — so a consumer sees exactly when the carrier was handed to the decision-directed discriminator or dropped back to NDA), then the carrier loop's "<prefix>.car.e" / ".freq" / ".locked" and the symbol-timing loop's "<prefix>.sync.e" / ".ctrl" / ".rate" / ".lock" / ".locked" / ".mu" -- eleven probes total, all thinned by decim and all emitted once per recovered symbol.  Passing NULL detaches everything. Setup path, never hot; the context is borrowed and must outlive the attachment (SPSC rules in telemetry/telemetry.h).

        Parameters
        ----------
        tlm : object | None
            Telemetry context to attach, or NULL to detach.
        prefix : str
            Probe-name prefix, e.g. "rx".
        decim : int
            Emit every decim-th symbol; >= 1.

        Examples
        --------
        >>> import numpy as np
        >>> from doppler.track import MpskReceiver
        >>> from doppler.telemetry import Telemetry
        >>> tlm = Telemetry(1 << 14)   # 11 probes x ~512 symbols, with headroom
        >>> rx = MpskReceiver(m=4, sps=4, m_out=2)
        >>> rx.set_telemetry(tlm, "rx")
        >>> len(tlm.probe_names())
        11
        >>> rng = np.random.default_rng(7)
        >>> syms = (1 - 2 * rng.integers(0, 2, 512)).astype(np.complex64)
        >>> x = np.repeat(syms, 4)
        >>> _ = rx.steps(x)
        >>> recs = tlm.read()
        >>> tlm.dropped        # size the ring, or the counts below diverge
        0
        >>> n_sync = len(recs[recs["probe"] == tlm.probe_id("rx.sync.e")])
        >>> n_car = len(recs[recs["probe"] == tlm.probe_id("rx.car.e")])
        >>> n_sync > 0 and n_sync == n_car
        True

        """

    def steps(self, x: NDArray[np.complex64], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Demodulate a cf32 block and return the recovered M-PSK symbols (one cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the receiver pushes x through the matched DDC -- LO mix, decimating cascade, and a terminal polyphase stage whose bank IS the matched filter and whose selected arm IS the fractional symbol-timing delay -- then folds every output that stage produced into two loops: a Gardner symbol-timing loop steering the cascade's rate_ctrl port, and a carrier loop steering the LO's freq_ctrl port. The carrier discriminator runs on the on-time strobe only -- a non-strobe output straddles two symbols, so its M-th power is intersymbol interference rather than carrier phase -- and while acquiring it is the non-data-aided M-th-power error, needing no data and no symbol timing. With acq_to_track enabled a verify-counted two-way handover steps on the carrier lock metric each symbol: it switches to a lower-jitter decision-directed carrier loop after 8 consecutive above-lock_thresh symbols, and on a sustained lock loss (32 consecutive symbols below 0.8*lock_thresh) drops back to the NDA acquisition steer, the shared loop filter carrying the frequency estimate both ways. The loop locks to one of m phases (M-fold ambiguity); resolve it with bits(differential) or a sync word. Read norm_freq for the tracked carrier and lock for the carrier lock metric.

        Runs the per-sample loop (mix + cascade + matched filter, then the
        carrier and timing loops) over x and writes one cf32 symbol per
        recovered symbol period — roughly `x_len / sps` outputs. Read norm_freq
        for the tracked carrier and lock for the carrier lock metric.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def bits(self, x: NDArray[np.complex64], out: NDArray[np.uint8] | None = None) -> NDArray[np.uint8]:
        """Demodulate a cf32 block and return hard Gray-coded bits (log2(m) bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if the receiver was created with differential=1, each symbol's bits come from the phase DIFFERENCE between consecutive symbols (rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the symbol-error rate). Same per-sample carrier/timing recovery as steps().

        Like mpsk_receiver_steps(), but each recovered symbol is sliced to its
        nearest M-PSK point and unpacked to log2(M) hard bits (LSB-first). With
        the differential option set at create time, the Gray label is taken from
        the phase *difference* between consecutive symbols (rotation-invariant —
        it resolves the M-fold carrier ambiguity), else from the absolute
        (coherent) decision.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.
        """

    def bits_max_out(self) -> int:
        """Max output length bits() can produce for the current state."""

    def configure_lock(self, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Re-tune the acquisition<->tracking handover detector: hands the carrier to the decision-directed discriminator after n_up consecutive symbols with the carrier lock EMA above up_thresh, and falls back to NDA acquisition after n_down consecutive symbols below down_thresh (level + time hysteresis; see detection.LockDet). Previously only settable at construction (lock_thresh, with fixed 0.8x drop / 8-up / 32-down constants) -- this is the post-construction re-tune Dll and Costas both already have. A live handover survives the re-tune; the in-flight verify run restarts.

        Full lockdet control over the handover, mirroring
        costas_configure_lock(): a split declare/drop threshold pair on the
        carrier lock EMA (level hysteresis) and both verify counts (time
        hysteresis). A live handover survives the re-tune; the in-flight verify
        run restarts.

        Parameters
        ----------
        up_thresh : float
            Declare threshold on the carrier lock EMA.
        down_thresh : float
            Drop threshold; choose <= up_thresh for level hysteresis.
        n_up : int
            Consecutive above-threshold symbols to hand over to the decision-directed discriminator; clamped >= 1.
        n_down : int
            Consecutive below-threshold symbols to fall back to NDA acquisition; clamped >= 1.

        Examples
        --------
        >>> from doppler.track import MpskReceiver
        >>> rx = MpskReceiver(m=4, sps=4, m_out=2, acq_to_track=1)
        >>> rx.tracking
        0
        >>> rx.configure_lock(0.9, 0.72, 4, 16)   # tighter declare, faster drop

        """

    def reset(self) -> None:
        """Re-seed the carrier and symbol-timing loops to their create-time state; preserve configuration.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def norm_freq(self) -> float:
        """Carrier frequency the receiver is tracking, cycles/sample at the input rate: the create-time centre plus the loop's own estimate."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock(self) -> float:
        """Lock."""

    @property
    def timing_rate(self) -> float:
        """Smoothed tracked samples per symbol — departs from the nominal `sps` by exactly the sample-clock offset the timing loop is tracking."""

    @property
    def tracking(self) -> int:
        """Tracking."""

    @property
    def m(self) -> int:
        """M."""

    @property
    def sps(self) -> float:
        """Sps."""

    @property
    def m_out(self) -> int:
        """Terminal outputs per symbol (the old `n`, now the cascade's)."""

    @property
    def clipped(self) -> int:
        """Has the cascade's CIC stage clipped its input since the last reset? A CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past that -- the output stays finite and plausible, merely distorted, at a cost of ~25 dB of EVM that no lock metric reveals. Always 0 for a plan with no CIC stage."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "MpskReceiver": ...

    def __exit__(self, *args: object) -> None: ...

@final
class MpskReceiverR:
    """Create a real-input M-PSK receiver.

    Parameters
    ----------
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).
    sps : float, default 32.0
        Samples per symbol. Any double **strictly greater than `2 * m_out`** (the cascade behind the R2C halfband runs at twice the overall rate, and Ddcr needs that below 0.5) -- 33.33389 is as valid as 32, because the front end plans its own cascade and the terminal stage's accumulator is a double. That is the real-world case whenever the ADC clock is free-running against the symbol clock. The default is 32 rather than the complex twin's 8 purely to clear that bound: `m_out` defaults to 8, so anything at or below 16 cannot construct.
    m_out : int, default 8
        Terminal outputs per symbol: even, 2..8. The Gardner detector takes every m_out-th output as the on-time strobe and the one m_out/2 back as the transition gate, so the oversampled matched-filtered stream falls out for free. **The default is 8 because that is where the I&D matched filter reaches the coherent bound.** The rectangle is one symbol wide, so its matched filter is an m_out-tap sum spanning it, and a smaller m_out samples that same integral more coarsely. Measured on the complex twin at its default sps=8 against the coherent bound EVM_dB = -(Es/N0)_dB: at 18 dB Es/N0, m_out=8 lands 0.41 dB off the bound where m_out=4 loses 3.11 dB; at 14 dB it is 0.25 dB against 1.71 dB -- the gap widens as noise stops hiding it. Because `sps` must clear `2 * m_out` here, this default is also what puts the `sps` default at 32. **Never pair 2 with pulse="iandd"**: the matched filter degenerates to a two-tap sum, the eye barely opens (measured lock statistic -0.34 at 2 against +0.95 at 4 on the same NRZ stream) and acquisition itself fails about half the time. Replaces the old `n` (NDA arm dumps per symbol): the cascade's own outputs now feed the carrier discriminator, so there is no separate arm to size.
    pulse : Literal["iandd", "rrc"], default "iandd"
        Matched-filter shape (default MPSK_RX_PULSE_IANDD).
    rrc_beta : float, default 0.35
        RRC roll-off in `[0, 1]` (default 0.35; RRC only).
    rrc_span : int, default 8
        RRC one-sided span in symbols (default 8; RRC only).
    bn_carrier : float, default 0.01
        Carrier loop noise bandwidth, normalised to the symbol rate (default 0.005).
    zeta : float, default 0.707
        Damping factor for both loops (default 0.707).
    bn_timing : float, default 0.01
        Timing loop noise bandwidth, per symbol (0.01).
    acq_to_track : int, default 0
        Enable the two-way handover (default 0).
    lock_thresh : float, default 0.5
        Handover declare threshold (default 0.5).
    init_norm_freq : float, default 0.0
        Carrier frequency to tune to, cycles/sample **at the real input rate** (default 0.0). A real IF at `0.2 * fs` is `0.2`; the halved value the LO actually uses is this object's business, not the caller's.
    warmup_syms : int, default 100
        Symbols before the handover is allowed (100).
    differential : int, default 0
        bits(): differential demap (default 0).
    num_phases : int, default 1024
        Matched-filter bank arms; a power of two. Sets the fractional-timing resolution to 1/num_phases of an output period. The bank is sized by the POST-decimation rate, so this costs the same at sps=8 and sps=256.
    nda_tap : Literal["strobe", "mf_all", "lo_arm"], default "strobe"
        Where the NDA carrier discriminator reads from, which sets its pull-in range and whether it needs symbol timing at all. An M-th-power detector updating at rate F can only see |df| < F/(2M), so the tap point IS the range. `strobe` (default) reads the on-time strobe at the symbol rate Rs: the cleanest input, the narrowest range (Rs/(2M)), and the only tap whose input quality depends on the timing loop -- it steers from its first strobe whether or not timing has declared, so when the carrier must acquire before timing does, that is a reason to pick another tap rather than something the receiver resolves for you. `mf_all` reads every terminal output at m_out*Rs -- m_out times the range and no timing dependence, paid for with the ISI the between-symbol outputs carry, which hurts most where the decision margin is smallest (8PSK). `lo_arm` reads ahead of the cascade through a free-running half-symbol boxcar at the LO rate -- the widest range and fully timing-independent, but unmatched, so it pays squaring loss. Fixed at construction: nothing switches underneath you. If you need more range than any tap gives, put a coarse frequency estimate in front and pass it as init_norm_freq.

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import MpskReceiverR
    >>> obj = MpskReceiverR(m=4, sps=32.0, m_out=8, pulse="iandd", rrc_beta=0.35, rrc_span=8, bn_carrier=0.01, zeta=0.707, bn_timing=0.01, acq_to_track=0, lock_thresh=0.5, init_norm_freq=0.0, warmup_syms=100, differential=0, num_phases=1024, nda_tap="strobe")

    """
    def __init__(self, m: int = ..., sps: float = ..., m_out: int = ..., pulse: Literal["iandd", "rrc"] = "iandd", rrc_beta: float = ..., rrc_span: int = ..., bn_carrier: float = ..., zeta: float = ..., bn_timing: float = ..., acq_to_track: int = ..., lock_thresh: float = ..., init_norm_freq: float = ..., warmup_syms: int = ..., differential: int = ..., num_phases: int = ..., nda_tap: Literal["strobe", "mf_all", "lo_arm"] = "strobe") -> None: ...

    def set_telemetry(self, tlm: object | None, prefix: str, decim: int = 1) -> None:
        """Attach (or detach) telemetry; registers the same eleven probes as mpsk_receiver_set_telemetry(), whose contract this shares.

        Parameters
        ----------
        tlm : object | None
            Input.
        prefix : str
            Input.
        decim : int
            Input.
        """

    def steps(self, x: NDArray[np.float32], out: NDArray[np.complex64] | None = None) -> NDArray[np.complex64]:
        """Demodulate a real f32 block and return the recovered M-PSK symbols (one cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the receiver pushes x through the matched DDCR -- LO mix, decimating cascade, and a terminal polyphase stage whose bank IS the matched filter and whose selected arm IS the fractional symbol-timing delay -- then folds every output that stage produced into two loops: a Gardner symbol-timing loop steering the cascade's rate_ctrl port, and a carrier loop steering the LO's freq_ctrl port. The carrier discriminator runs on the on-time strobe only -- a non-strobe output straddles two symbols, so its M-th power is intersymbol interference rather than carrier phase -- and while acquiring it is the non-data-aided M-th-power error, needing no data and no symbol timing. With acq_to_track enabled a verify-counted two-way handover steps on the carrier lock metric each symbol: it switches to a lower-jitter decision-directed carrier loop after 8 consecutive above-lock_thresh symbols, and on a sustained lock loss (32 consecutive symbols below 0.8*lock_thresh) drops back to the NDA acquisition steer, the shared loop filter carrying the frequency estimate both ways. The loop locks to one of m phases (M-fold ambiguity); resolve it with bits(differential) or a sync word. Read norm_freq for the tracked carrier and lock for the carrier lock metric.

        As mpsk_receiver_steps(), taking real samples: the R2C halfband makes
        them complex before anything else touches them.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real f32 input samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.
        """

    def steps_max_out(self) -> int:
        """Max output length steps() can produce for the current state."""

    def bits(self, x: NDArray[np.float32], out: NDArray[np.uint8] | None = None) -> NDArray[np.uint8]:
        """Demodulate a real f32 block and return hard Gray-coded bits (log2(m) bytes of 0/1 per recovered symbol, LSB-first). Coherent by default; if the receiver was created with differential=1, each symbol's bits come from the phase DIFFERENCE between consecutive symbols (rotation-invariant — resolves the m-fold carrier ambiguity at ~2x the symbol-error rate). Same per-sample carrier/timing recovery as steps().

        As mpsk_receiver_bits(), taking real samples.

        Parameters
        ----------
        x : NDArray[np.float32]
            Real f32 input samples.

        Returns
        -------
        NDArray[np.uint8]
            Number of bits written.
        """

    def bits_max_out(self) -> int:
        """Max output length bits() can produce for the current state."""

    def configure_lock(self, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Re-tune the acquisition<->tracking handover detector: hands the carrier to the decision-directed discriminator after n_up consecutive symbols with the carrier lock EMA above up_thresh, and falls back to NDA acquisition after n_down consecutive symbols below down_thresh (level + time hysteresis; see detection.LockDet). Previously only settable at construction (lock_thresh, with fixed 0.8x drop / 8-up / 32-down constants) -- this is the post-construction re-tune Dll and Costas both already have. A live handover survives the re-tune; the in-flight verify run restarts.

        Parameters
        ----------
        up_thresh : float
            Input.
        down_thresh : float
            Input.
        n_up : int
            Input.
        n_down : int
            Input.
        """

    def reset(self) -> None:
        """Re-seed the carrier and symbol-timing loops to their create-time state; preserve configuration.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def norm_freq(self) -> float:
        """Tracked carrier, cycles/sample at the REAL input rate."""
    @norm_freq.setter
    def norm_freq(self, value: float) -> None: ...

    @property
    def lock(self) -> float:
        """Lock."""

    @property
    def timing_rate(self) -> float:
        """Timing rate."""

    @property
    def tracking(self) -> int:
        """Tracking."""

    @property
    def m(self) -> int:
        """M."""

    @property
    def sps(self) -> float:
        """Sps."""

    @property
    def m_out(self) -> int:
        """M out."""

    @property
    def clipped(self) -> int:
        """Has the cascade's CIC stage clipped its input since the last reset? A CIC bounds its input to |Re|, |Im| <= 1.0 and clips silently past that -- the output stays finite and plausible, merely distorted, at a cost of ~25 dB of EVM that no lock metric reveals. Always 0 for a plan with no CIC stage."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "MpskReceiverR": ...

    def __exit__(self, *args: object) -> None: ...
