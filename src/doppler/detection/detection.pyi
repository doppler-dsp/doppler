# detection/detection.pyi — type stubs for the detection C extension.
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
