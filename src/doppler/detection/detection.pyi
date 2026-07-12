# detection/detection.pyi — type stubs for the detection C extension.
import numpy as np
from numpy.typing import NDArray

class LockDet:
    """LockDet component.

    Parameters
    ----------
    up_thresh : float, default 1.0
        up_thresh constructor parameter.
    down_thresh : float, default 1.0
        down_thresh constructor parameter.
    n_up : int, default 1
        n_up constructor parameter.
    n_down : int, default 1
        n_down constructor parameter.

    Examples
    --------
    Create with defaults:

    >>> from doppler.detection import LockDet
    >>> obj = LockDet(up_thresh=1.0, down_thresh=1.0, n_up=1, n_down=1)

    """
    def __init__(self, up_thresh: float = ..., down_thresh: float = ..., n_up: int = ..., n_down: int = ...) -> None: ...

    def step(self, x: float) -> int:
        """Feed one look of the lock metric; return the current decision.

        Unlocked: a hit (`x > up_thresh`) advances the verify run and the
        n_up-th consecutive hit declares lock; any miss resets the run. Locked:
        a miss (`x < down_thresh`) advances the run and the n_down-th
        consecutive miss drops the lock; any hit (`x >= down_thresh`) resets it.
        A metric inside the `[down_thresh, up_thresh]` band is sticky — it
        neither advances a declare nor a drop.

        Parameters
        ----------
        x : float
            Lock metric for this look.

        Returns
        -------
        int
            Decision after this look (1 = locked, 0 = not).

        Examples
        --------
        >>> from doppler.detection import LockDet
        >>> d = LockDet(up_thresh=1.5, down_thresh=1.2, n_up=2, n_down=3)
        >>> [d.step(2.0), d.step(2.0)]     # declared on the 2nd straight hit
        [0, 1]
        >>> d.step(1.3)                    # in the hysteresis band: stays up
        1
        >>> [d.step(1.0), d.step(1.0), d.step(1.0)]  # 3rd straight miss drops
        [1, 1, 0]

        """

    def steps(self, x: NDArray[np.float64], out: NDArray[np.int32] | None = None) -> NDArray[np.int32]:
        """Run a block of lock-metric looks through the detector.

        Parameters
        ----------
        x : NDArray[np.float64]
            Input.

        Returns
        -------
        NDArray[np.int32]
            Output.
        """

    def configure(self, up_thresh: float, down_thresh: float, n_up: int, n_down: int) -> None:
        """Re-tune thresholds and verify counts; a live lock survives, the in-flight verify run restarts under the new config.

        The current locked flag survives (a live lock is not dropped by a
        re-tune); the in-flight verify counter is cleared so the next run is
        counted entirely under the new config.

        Parameters
        ----------
        up_thresh : float
            Declare threshold (hit when metric > up_thresh).
        down_thresh : float
            Drop threshold (miss when metric < down_thresh).
        n_up : int
            Consecutive hits to declare; clamped to >= 1.
        n_down : int
            Consecutive misses to drop; clamped to >= 1.
        """

    def reset(self) -> None:
        """Drop the lock and clear the verify counter; keep the config.
        """

    def state_bytes(self) -> int:
        """Serialized state size in bytes."""
    def get_state(self) -> bytes:
        """Serialize the engine's mutable state to bytes."""
    def set_state(self, blob: bytes) -> None:
        """Restore mutable state from a get_state() blob."""

    @property
    def up_thresh(self) -> float:
        """Up thresh."""
    @up_thresh.setter
    def up_thresh(self, value: float) -> None: ...

    @property
    def down_thresh(self) -> float:
        """Down thresh."""
    @down_thresh.setter
    def down_thresh(self, value: float) -> None: ...

    @property
    def n_up(self) -> int:
        """N up."""

    @property
    def n_down(self) -> int:
        """N down."""

    @property
    def cnt(self) -> int:
        """Running consecutive-look verify counter: hits toward a declare while unlocked, misses toward a drop while locked."""

    @property
    def locked(self) -> bool:
        """Current decision (True = locked)."""

    def destroy(self) -> None:
        """Release C resources immediately."""

    def __enter__(self) -> "LockDet": ...

    def __exit__(self, *args: object) -> None: ...

def marcum_q(m: int, a: float, b: float) -> float:
    """Marcum Q function Q_M(a, b) for integer M >= 1.

    Probability that a Rice(a, sigma=1) random variable exceeds b. For M=1:
    Q_1(a, b) = P(Rice(a,1) > b). General integer M relates to the
    noncentral chi-squared CDF with 2M degrees of freedom.

    Computed via the Poisson-weighted chi-squared series (exact for M=1,
    converges in ~60 terms for practical a, b <= 15):

    Q_M(a, b) = sum_{k=0}^inf w_k * Q_{M+k}(0, b)

    where: w_k = exp(-u) * u^k/k! (u = a^2/2) Q_n(0,b) = exp(-v) *
    sum_{j=0}^{n-1} v^j/j! (v = b^2/2)

    Each iteration advances both the Poisson weight and the chi-sum in O(1)
    using the recurrences w_{k+1} = w_k * u/(k+1) and Q_{n+1}(0,b) =
    Q_n(0,b) + exp(-v)*v^n/n!. Total cost: O(K) where K ~ max(u, M) + safety
    margin.

    Special cases: - a = 0: Q_M(0, b) = exp(-b^2/2) * sum_{j=0}^{M-1}
    (b^2/2)^j/j! - b <= 0: Q_M(a, b) = 1.0

    Parameters
    ----------
    m : int
        Integration order; must be >= 1.
    a : float
        Non-centrality parameter (signal strength).  a = 0 for H0.
    b : float
        Threshold (same units as test_stat).

    Returns
    -------
    float
        Q_M(a, b) in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import marcum_q
    >>> round(marcum_q(m=1, a=0.0, b=1.0), 5)   # P(Rayleigh > 1) = exp(-0.5)
    0.60653
    >>> round(marcum_q(m=1, a=0.0, b=2.0), 5)   # exp(-2)
    0.13534
    >>> round(marcum_q(m=2, a=0.0, b=2.0), 5)   # 3*exp(-2)
    0.40601
    >>> round(marcum_q(m=1, a=2.0, b=1.0), 5)   # signal present (a=2)
    0.91811

    """

def det_threshold(pfa: float) -> float:
    """Threshold eta for a given false-alarm probability.

    Exact closed-form inversion of Pfa = exp(-eta^2/2):

    eta = sqrt(-2 * ln(pfa))

    The threshold is independent of dwell and SNR; it depends only on the
    desired Pfa.

    Parameters
    ----------
    pfa : float
        Desired false-alarm probability; must be in (0, 1).

    Returns
    -------
    float
        Threshold eta > 0.

    Examples
    --------
    >>> from doppler.detection import det_threshold
    >>> round(det_threshold(pfa=1e-6), 4)
    5.2565

    """

def det_pd(snr: float, dwell: int, threshold: float) -> float:
    """Detection probability for given per-sample amplitude SNR and dwell.

    Computes Pd = Q_1(a, eta) where a = sqrt(2 * dwell) * snr.

    At snr = 0, det_pd returns Pfa (the false-alarm rate, as expected for a
    noise-only input). As snr or dwell increase, Pd approaches 1.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (signal / noise amplitude, linear).  snr = 0 gives Pd = Pfa.
    dwell : int
        Coherent integration depth; must be >= 1.
    threshold : float
        Test-stat threshold eta, e.g. from det_threshold().

    Returns
    -------
    float
        Detection probability in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import det_pd, det_threshold
    >>> thr = det_threshold(pfa=1e-6)
    >>> round(det_pd(snr=1.613, dwell=8, threshold=thr), 2)  # 8-dwell -> Pd~0.9
    0.9
    >>> round(det_pd(snr=0.0, dwell=8, threshold=thr), 6)    # snr=0 -> Pd=Pfa
    1e-06

    """

def det_dwell(snr: float, pd_min: float, pfa: float, max_dwell: int) -> int:
    """Minimum dwell such that Pd >= pd_min for the given SNR and Pfa.

    Iterates dwell = 1, 2, ..., max_dwell, computing det_pd() at each step.
    Returns the first dwell that satisfies the Pd requirement, or -1 if none
    is found within max_dwell iterations.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (linear).
    pd_min : float
        Required detection probability, e.g. 0.9.
    pfa : float
        False-alarm probability; used to derive eta.
    max_dwell : int
        Search upper bound; prevents infinite loops for low SNR.

    Returns
    -------
    int
        Minimum dwell >= 1, or -1 if not achievable.

    Examples
    --------
    >>> from doppler.detection import det_dwell
    >>> det_dwell(snr=0.5, pd_min=0.9, pfa=1e-6, max_dwell=256)
    84

    """

def det_snr(dwell: int, pd_min: float, pfa: float) -> float:
    """Minimum per-sample amplitude SNR achieving Pd >= pd_min.

    Binary search over SNR in &#91;0, hi&#93; where hi is doubled from 1.0
    until det_pd(hi, dwell, threshold) >= pd_min. 64 bisection iterations
    yield ~1e-19 relative precision on the final interval.

    Parameters
    ----------
    dwell : int
        Coherent integration depth; must be >= 1.
    pd_min : float
        Required detection probability.
    pfa : float
        False-alarm probability; used to derive eta.

    Returns
    -------
    float
        Minimum amplitude SNR >= 0.

    Examples
    --------
    >>> from doppler.detection import det_snr, det_pd, det_threshold
    >>> snr = det_snr(dwell=8, pd_min=0.9, pfa=1e-6)
    >>> round(snr, 3)
    1.613
    >>> det_pd(snr=snr, dwell=8, threshold=det_threshold(pfa=1e-6)) >= 0.9
    True

    """

def det_threshold_noncoherent(pfa: float, n_noncoh: int) -> float:
    """CFAR threshold eta_nc for a non-coherent detector of n_noncoh looks.

    Solves marcum_q(n_noncoh, 0, eta_nc) = pfa (the order-M central tail,
    monotone decreasing in eta_nc) by bisection. For n_noncoh = 1 this is
    the exact closed form sqrt(-2 ln pfa) (== det_threshold).

    Parameters
    ----------
    pfa : float
        Per-test false-alarm probability in (0, 1).
    n_noncoh : int
        Number of non-coherent looks; must be >= 1.

    Returns
    -------
    float
        Threshold eta_nc on the normalized statistic R.

    Examples
    --------
    >>> from doppler.detection import det_threshold_noncoherent, det_threshold
    >>> round(det_threshold_noncoherent(pfa=1e-3, n_noncoh=4), 3)
    5.111
    >>> det_threshold_noncoherent(pfa=1e-6, n_noncoh=1) == det_threshold(pfa=1e-6)
    True

    """

def det_ema_alpha(snr_in_db: float, snr_out_db: float) -> float:
    """EMA coefficient for a target estimator SNR (DC level in noise).

    Sizes a first-order EMA `y = (1-alpha)*y + alpha*x` that estimates a DC
    level from noisy i.i.d. measurements x. Per sample the estimator SNR
    (mean^2 / variance) is `snr_in`; the EMA improves it by its variance
    reduction `(2-alpha)/alpha`, so the output SNR is `snr_out = snr_in *
    (2-alpha)/alpha`. Solving for the coefficient:

    alpha = 2 * snr_in / (snr_in + snr_out) (SNRs linear)

    Returns 1.0 (no averaging) when snr_out_db <= snr_in_db. Typical inputs:
    a signal-free power reference |n|^2 is exponential (0 dB per sample); a
    lock signal at known C/N0 has per-look SNR from its coherent integration
    (minus squaring loss), and this picks the smoothing bandwidth that makes
    the lock decision variable meet a chosen decision SNR.

    Parameters
    ----------
    snr_in_db : float
        Per-sample estimator SNR, dB (mean^2 / variance).
    snr_out_db : float
        Desired EMA-output estimator SNR, dB.

    Returns
    -------
    float
        EMA coefficient alpha in (0, 1].

    Examples
    --------
    >>> from doppler.detection import det_ema_alpha
    >>> det_ema_alpha(0.0, 0.0)      # no gain requested -> no averaging
    1.0
    >>> round(1 / det_ema_alpha(0.0, 20.0), 1)   # 20 dB gain ~ 50 looks
    50.5
    >>> round(1 / det_ema_alpha(10.0, 30.0), 1)  # same 20 dB gain, shifted
    50.5

    """

def det_verify_count(p_look: float, p_target: float) -> int:
    """Verify count: consecutive looks needed to compound to a budget.

    n consecutive independent looks at per-look probability p compound to
    p^n, so the smallest n with `p_look^n <= p_target` is `ceil(ln p_target
    / ln p_look)` (clamped to >= 1). One function serves both sides of a
    lock detector (lockdet_core.h): the declare count from (per-look pfa,
    false-declare budget) and the drop count from (per-look miss rate 1 -
    pd, false-drop budget). Degenerate inputs resolve naturally: a target
    already met by one look returns 1; p_look >= 1 can never compound below
    a smaller target and returns INT_MAX.

    Parameters
    ----------
    p_look : float
        Per-look probability (pfa or 1 - pd), in (0, 1).
    p_target : float
        Compound probability budget, in (0, 1).

    Returns
    -------
    int
        Smallest verify count n with p_look^n <= p_target.

    Examples
    --------
    >>> from doppler.detection import det_verify_count
    >>> det_verify_count(1e-3, 1e-6)   # two 1e-3 looks reach 1e-6
    2
    >>> det_verify_count(1e-3, 1e-9)
    3
    >>> det_verify_count(0.5, 1e-3)    # drop side: pd = 0.5 per look
    10
    >>> det_verify_count(1e-3, 0.5)    # budget already met -> 1
    1

    """

def det_verify_delay(p_look: float, n: int) -> float:
    """Expected looks until a run of n consecutive successes completes.

    The mean waiting time of the consecutive-run process a lockdet verify
    counter implements: at per-look success probability p, the first run of
    n straight successes takes on average

    `E[T]` = (1 - p^n) / (p^n * (1 - p)) looks,

    which is the declare latency bought by a verify count of n (multiply by
    the look period for time). Limits are handled exactly: p = 1 gives n
    (the run completes immediately), p = 0 gives infinity.

    Parameters
    ----------
    p_look : float
        Per-look success probability (e.g. pd), in &#91;0, 1&#93;.
    n : int
        Run length (the verify count); clamped to >= 1.

    Returns
    -------
    float
        Expected number of looks to the first length-n run.

    Examples
    --------
    >>> from doppler.detection import det_verify_delay
    >>> det_verify_delay(1.0, 8)             # certain hits: exactly n
    8.0
    >>> round(det_verify_delay(0.5, 2), 6)   # 2 straight coin heads: 6
    6.0
    >>> round(det_verify_delay(0.9, 8), 1)
    13.2

    """

def det_threshold_f(pfa: float, n: int) -> float:
    """Upper quantile of F(n, n) — the exact H0 law for a ratio test whose noise reference is estimated from as many samples as the signal sum.

    A chi-square threshold (det_threshold_noncoherent) prices a statistic
    normalised by a KNOWN noise power. When the noise power is instead
    estimated from n same-burst samples (the BurstDespreader lock test: sum
    Re^2 against sum Im^2), the ratio's tail fattens to F(n, n) and the
    chi-square gate realizes tens of times the priced pfa (41x at n = 16,
    pfa = 1e-3). This helper returns the exact gate: P(chi2_n / chi2_n > g)
    = I_{1/(1+g)}(n/2, n/2) = pfa, solved on the regularized incomplete beta
    — valid for every n >= 1, odd included. As n grows the estimate hardens
    and g approaches the known-noise value. Threshold a BurstDespreader as
    `lock_stat > sqrt(stat_n * det_threshold_f(pfa, stat_n))`.

    Parameters
    ----------
    pfa : float
        Tail probability budget, in (0, 1).
    n : int
        Degrees of freedom on each side (>= 1).

    Returns
    -------
    float
        The F(n, n) upper-pfa quantile; 0 on invalid input.

    Examples
    --------
    >>> from doppler.detection import det_threshold_f
    >>> round(det_threshold_f(1e-3, 2), 6)  # exact: (1 - pfa)/pfa
    999.0
    >>> round(det_threshold_f(1e-3, 4), 4)
    53.4358
    >>> round(det_threshold_f(1e-3, 64), 4)  # hardens toward known-noise
    2.1931

    """

def det_pd_noncoherent(snr: float, n_coh: int, n_noncoh: int, threshold: float) -> float:
    """Detection probability for n_noncoh non-coherent looks.

    Computes Pd = Q_{n_noncoh}(a, threshold) with the non-centrality a =
    sqrt(2 * n_coh * n_noncoh) * snr. At n_noncoh = 1 this is exactly
    det_pd(snr, n_coh, threshold); at snr = 0 it returns the per-test Pfa.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (signal / noise amplitude).
    n_coh : int
        Coherent integration length in samples (dwell * N).
    n_noncoh : int
        Number of non-coherent looks; must be >= 1.
    threshold : float
        Threshold eta_nc, e.g. from det_threshold_noncoherent().

    Returns
    -------
    float
        Detection probability in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import det_pd_noncoherent, det_pd, det_threshold
    >>> from doppler.detection import det_threshold_noncoherent
    >>> eta = det_threshold(pfa=1e-6)
    >>> det_pd_noncoherent(snr=0.5, n_coh=8, n_noncoh=1, threshold=eta) \
    ...     == det_pd(snr=0.5, dwell=8, threshold=eta)        # reduces to coherent
    True
    >>> eta4 = det_threshold_noncoherent(pfa=1e-3, n_noncoh=4)
    >>> round(det_pd_noncoherent(snr=0.3, n_coh=16, n_noncoh=4, threshold=eta4), 2)
    0.19

    """

def det_n_noncoh(snr: float, n_coh: int, pd_min: float, pfa: float, max_n_noncoh: int) -> int:
    """Minimum non-coherent looks achieving Pd >= pd_min at fixed n_coh.

    Iterates n_noncoh = 1, 2, ..., max_n_noncoh, recomputing the threshold
    (det_threshold_noncoherent, which grows with the look count) at each
    step. Returns the first look count that meets the Pd requirement, or -1
    if none does within max_n_noncoh. Used by the acquisition engine's (M,
    N_nc) split.

    Parameters
    ----------
    snr : float
        Per-sample amplitude SNR (linear).
    n_coh : int
        Coherent integration length in samples (dwell * N).
    pd_min : float
        Required detection probability, e.g. 0.9.
    pfa : float
        Per-test false-alarm probability.
    max_n_noncoh : int
        Search upper bound on the look count.

    Returns
    -------
    int
        Minimum n_noncoh >= 1, or -1 if not achievable.

    Examples
    --------
    >>> from doppler.detection import det_n_noncoh
    >>> det_n_noncoh(snr=2.0, n_coh=16, pd_min=0.9, pfa=1e-3, max_n_noncoh=64)
    1

    """

def det_threshold_power(pfa: float) -> float:
    """Power threshold p from Pfa for the power detector.

    Exact closed-form: P(Exponential(1) > p) = exp(-p) = Pfa, so

    p = -ln(Pfa)

    Parameters
    ----------
    pfa : float
        Desired false-alarm probability; must be in (0, 1).

    Returns
    -------
    float
        Threshold p > 0.

    Examples
    --------
    >>> from doppler.detection import det_threshold_power
    >>> round(det_threshold_power(pfa=1e-6), 3)   # -ln(1e-6) = 6*ln(10)
    13.816

    """

def det_pd_power(snr_power: float, dwell: int, power_threshold: float) -> float:
    """Detection probability for the power detector.

    Pd = Q_1(sqrt(2·dwell·snr_power), sqrt(2·power_threshold))

    The result equals det_pd() at the equivalent amplitude SNR: power SNR
    `s` corresponds to amplitude SNR `sqrt(s)`, and the Q_1 arguments match.

    Parameters
    ----------
    snr_power : float
        Per-sample power SNR (signal power / noise power at the correlator output, linear).  0 gives Pd = Pfa.
    dwell : int
        Coherent integration depth; must be >= 1.
    power_threshold : float
        Threshold p, e.g. from det_threshold_power().

    Returns
    -------
    float
        Detection probability in &#91;0, 1&#93;.

    Examples
    --------
    >>> from doppler.detection import det_pd_power, det_threshold_power
    >>> thr = det_threshold_power(pfa=1e-6)
    >>> round(det_pd_power(snr_power=2.6017, dwell=8, power_threshold=thr), 2)
    0.9

    """

def det_dwell_power(snr_power: float, pd_min: float, pfa: float, max_dwell: int) -> int:
    """Minimum dwell such that Pd >= pd_min for the power detector.

    Parameters
    ----------
    snr_power : float
        Per-sample power SNR (linear).
    pd_min : float
        Required detection probability.
    pfa : float
        False-alarm probability; used to derive p.
    max_dwell : int
        Search upper bound.

    Returns
    -------
    int
        Minimum dwell >= 1, or -1 if not achievable.

    Examples
    --------
    >>> from doppler.detection import det_dwell_power
    >>> det_dwell_power(snr_power=0.25, pd_min=0.9, pfa=1e-6, max_dwell=256)
    84

    """

def det_snr_power(dwell: int, pd_min: float, pfa: float) -> float:
    """Minimum per-sample power SNR achieving Pd >= pd_min.

    Parameters
    ----------
    dwell : int
        Coherent integration depth; must be >= 1.
    pd_min : float
        Required detection probability.
    pfa : float
        False-alarm probability.

    Returns
    -------
    float
        Minimum power SNR >= 0.

    Examples
    --------
    >>> from doppler.detection import (det_snr_power, det_pd_power,
    ...                                det_threshold_power)
    >>> sp = det_snr_power(dwell=8, pd_min=0.9, pfa=1e-6)
    >>> round(sp, 4)
    2.6017
    >>> det_pd_power(snr_power=sp, dwell=8,
    ...              power_threshold=det_threshold_power(pfa=1e-6)) >= 0.9
    True

    """
