# track/track.pyi — type stubs for the track C extension.
from typing import Literal
import numpy as np
from numpy.typing import NDArray

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

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
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

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "Costas": ...

    def __exit__(self, *args: object) -> None: ...

class Dll:
    """Create a DLL instance (COPIES code).

    Parameters
    ----------
    code : NDArray[np.uint8], default ...
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

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Correlate a cf32 block against the local code with early/prompt/late taps and steer the code NCO each code period on the non-coherent (sum|E|-sum|L|)/(sum|E|+sum|L|) discriminator. With segments=1 (default) this is a coherent full-epoch integrate-and-dump: one prompt symbol per period. With segments>1 each epoch is split into that many sub-epoch partial correlations: it emits that many partial prompts per period (a stream at ~segments samples/symbol when the symbol rate is near the code rate) and tracks the code non-coherently across the partials, which a data flip cannot collapse (robust to an asynchronous data-symbol clock). segments>1 is the streaming despreader: it removes the PN code and outputs samples. The non-coherent loop is carrier-blind, so it tracks with a residual carrier still on the input; carrier recovery (Costas) and symbol-timing recovery (SymbolSync) are downstream stages fed from the partial output. The output is an independent array per call (block-size invariant).

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
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

    def configure_lock(self, pfa: float, n_looks: int) -> None:
        """Tune the always-on code-lock detector to a target (pfa, n_looks). The detector reuses acquisition's non-coherent statistic R = sqrt(2*sum|P|^2 / E|O|^2), where the prompt powers of n_looks consecutive looks are summed and E|O|^2 is an EMA of a random off-peak (noise) correlation re-drawn each epoch; it declares lock when R exceeds det_threshold_noncoherent(pfa, n_looks). Size n_looks with detection.det_n_noncoh(snr, ...) for your operating C/N0. The default is pfa=1e-3 over 20 looks. Read the result from the locked / lock_stat / noise_est properties.

        The DLL carries a lock detector that reuses acquisition's non-coherent
        test statistic. Every emitted look (a partial in segments mode, or the
        full-epoch prompt when segments == 1) is also correlated at a *random
        off-peak* code phase — re-drawn each epoch and kept `noise_guard` chips
        clear of the prompt/early/late lobe — to give a signal-free CFAR noise
        sample (valid for a low-sidelobe code, e.g. Gold). The offset power
        feeds an EMA reference `E|O|^2`; the prompt powers of n_looks
        consecutive looks are summed into `S = sum|P_k|^2`, and the detector
        declares lock when

        R = sqrt(2 * S / E|O|^2) > threshold

        which under H0 has `P(R > threshold) = marcum_q(n_looks, 0, threshold)`
        — so a caller sizes threshold = det_threshold_noncoherent(pfa, n_looks)
        and n_looks = det_n_noncoh(snr, ...) to meet a target (Pfa, Pd). The
        threshold is passed in (not derived) so the core stays dependency-free;
        the Python binding converts a `pfa` via the detection module. The EMA
        must average many more cells than the test integrates (`1/alpha >>
        n_looks`) or the noise estimate's own variance inflates Pfa; the binding
        defaults `1/alpha` to `max(1024, 32*n_looks)`.

        Parameters
        ----------
        pfa : float
            Input.
        n_looks : int
            Non-coherent integration depth N (looks); clamped to >= 1.
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
        """True when the code-lock detector's statistic exceeds its CFAR threshold (latched at each n_looks-look decision; see configure_lock)."""

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

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import SymbolSync
    >>> obj = SymbolSync(sps=4, bn=0.01, zeta=0.707, order="cubic")

    """
    def __init__(self, sps: int = ..., bn: float = ..., zeta: float = ..., order: Literal["linear", "parabolic", "cubic"] = "cubic") -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Recover symbol timing from an oversampled cf32 baseband block: a Gardner timing-error detector drives an integer timing NCO whose post-wrap value gives the interpolation fraction for free, and a Farrow interpolator emits one symbol-rate sample per recovered symbol instant.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input.

        Returns
        -------
        NDArray[np.complex64]
            Output.
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

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "SymbolSync": ...

    def __exit__(self, *args: object) -> None: ...

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

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
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

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
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

class MpskReceiver:
    """Create an M-PSK receiver.

    Parameters
    ----------
    m : int, default 4
        Constellation order M, 2/4/8 (default 4 = QPSK).
    sps : int, default 8
        Samples per symbol (default 8).
    n : int, default 4
        Carrier arm dumps per symbol (default 4; sps % n == 0).
    pulse : Literal["iandd", "rrc"], default "iandd"
        Matched-filter shape (default MPSK_RX_PULSE_IANDD).
    rrc_beta : float, default 0.35
        RRC roll-off in [0, 1] (default 0.35; RRC only).
    rrc_span : int, default 8
        RRC one-sided span in symbols (default 8; RRC only).
    bn_carrier : float, default 0.01
        Carrier loop noise bandwidth (default 0.01).
    zeta : float, default 0.707
        Damping factor for both loops (default 0.707).
    bn_timing : float, default 0.01
        Symbol-timing loop noise bandwidth (default 0.01).
    auto_handover : int, default 0
        Enable NDA->decision-directed handover (default 0).
    lock_thresh : float, default 0.5
        Lock metric required for handover (default 0.5).
    init_norm_freq : float, default 0.0
        Seed carrier frequency, cycles/sample (default 0.0).
    warmup_syms : int, default 100
        Symbols before handover is allowed (default 100).
    differential : int, default 0
        bits(): differential (rotation-invariant) demap (default 0 = coherent).

    Examples
    --------
    Create with defaults:

    >>> from doppler.track import MpskReceiver
    >>> obj = MpskReceiver(m=4, sps=8, n=4, pulse="iandd", rrc_beta=0.35, rrc_span=8, bn_carrier=0.01, zeta=0.707, bn_timing=0.01, auto_handover=0, lock_thresh=0.5, init_norm_freq=0.0, warmup_syms=100, differential=0)

    """
    def __init__(self, m: int = ..., sps: int = ..., n: int = ..., pulse: Literal["iandd", "rrc"] = "iandd", rrc_beta: float = ..., rrc_span: int = ..., bn_carrier: float = ..., zeta: float = ..., bn_timing: float = ..., auto_handover: int = ..., lock_thresh: float = ..., init_norm_freq: float = ..., warmup_syms: int = ..., differential: int = ...) -> None: ...

    def steps(self, x: NDArray[np.complex64]) -> NDArray[np.complex64]:
        """Demodulate a cf32 block and return the recovered M-PSK symbols (one cf32 per recovered symbol period, ~ len(x)/sps outputs). Per sample the receiver de-rotates with the integer-NCO carrier (predetection wipe-off), accumulates a non-data-aided M-th-power I/Q arm at n dumps/symbol to acquire the carrier with no data and no symbol timing, matched-filters the de-rotated stream (integrate-and-dump or RRC), and runs a Gardner symbol-timing loop. With auto_handover enabled it switches to a lower-jitter decision-directed carrier loop once locked. The loop locks to one of m phases (M-fold ambiguity); resolve it with bits(differential) or a sync word. Read norm_freq for the tracked carrier and lock for the carrier lock metric.

        Runs the per-sample loop (carrier wipe-off + NDA arm + matched filter +
        Gardner timing) over x and writes one cf32 symbol per recovered symbol
        period. Fewer outputs than inputs (~ x_len / sps). Read norm_freq for
        the tracked carrier and lock for the carrier lock metric.

        Parameters
        ----------
        x : NDArray[np.complex64]
            Input cf32 samples.

        Returns
        -------
        NDArray[np.complex64]
            Number of symbols written.
        """

    def bits(self, x: NDArray[np.complex64]) -> NDArray[np.uint8]:
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
        """Norm freq."""
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
    def sps(self) -> int:
        """Sps."""

    @property
    def n(self) -> int:
        """N."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "MpskReceiver": ...

    def __exit__(self, *args: object) -> None: ...
