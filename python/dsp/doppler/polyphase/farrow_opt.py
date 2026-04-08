"""
doppler.polyphase.farrow_opt
======================================

Polynomial-Based Filter (PBF) minimax optimization via linear
programming.

Translated from the MATLAB appendix of:
  M. T. Hunter, "Novel Farrow Structures for Sample Rate Conversion"
  PhD dissertation, University of Central Florida, 2009.

The optimizer finds the coefficient matrix C of shape (N, M+1) that
minimizes the Chebyshev (minimax) frequency-domain error subject to
optional time-domain continuity constraints.  The resulting filter
implements the DPMFS when used with J=2 polyphase decomposition.

Basis function convention (DPMFS):  a=1, b=-1/2
    ψ_m(n, T, t) = (a·μ + b)^m  where  μ = (t/T - n) ∈ [0, 1)

Usage
-----
    from doppler.polyphase.farrow_opt import optimize_dpmfs
    coeffs = optimize_dpmfs(
        passband=0.4, stopband=0.6,
        N=6, M=3, num_der=-1,
    )
    print(coeffs.to_c_header())
"""

from __future__ import annotations

import math
from typing import Optional

import numpy as np

try:
    from scipy.optimize import linprog as _linprog
except ImportError:
    _linprog = None  # type: ignore[assignment]

from ._dpmfs import DPMFSCoeffs

__all__ = ["optimize_dpmfs", "optimize_pbf", "pbf_freq_resp", "pbf_imp_resp"]


# ------------------------------------------------------------------ #
# Internal helpers                                                    #
# ------------------------------------------------------------------ #


def _fact(n: int) -> float:
    return float(math.factorial(int(n)))


def _psi_hat_even(
    M: int, N: int, omega_p: np.ndarray, a: float, b: float
) -> np.ndarray:
    """Even PBF basis frequency-response matrix Ψ̂_E(ω).

    Exploits linear-phase symmetry; returns shape (P, N//2*(M+1)),
    real-valued.  Equation (3.70) of Hunter 2009.
    """
    half = N // 2
    P = len(omega_p)
    out = np.zeros((P, half * (M + 1)))

    for n in range(half):
        e_neg_n = np.exp(-1j * omega_p * n)  # (P,)
        e_pos_h = np.exp(1j * omega_p * half)  # (P,)
        for m in range(M + 1):
            temp = np.zeros(P, dtype=complex)
            for k in range(m + 1):
                c_mk = _fact(m) / _fact(m - k)
                temp += (
                    e_neg_n
                    * (a**k * c_mk)
                    * (1j * omega_p) ** (-(k + 1))
                    * (b ** (m - k) - np.exp(-1j * omega_p) * (a + b) ** (m - k))
                )
            col = n * (M + 1) + m
            out[:, col] = 2.0 * np.real(e_pos_h * temp)

    return out


def _psi_hat_omega(
    M: int, N: int, omega_p: np.ndarray, a: float, b: float
) -> np.ndarray:
    """Full PBF basis frequency-response matrix Ψ̂(ω).

    Returns shape (P, N*(M+1)), complex-valued.
    Equation (3.60) of Hunter 2009.
    """
    P = len(omega_p)
    out = np.zeros((P, N * (M + 1)), dtype=complex)

    for n in range(N):
        e_neg_n = np.exp(-1j * omega_p * n)  # (P,)
        for m in range(M + 1):
            temp = np.zeros(P, dtype=complex)
            for k in range(m + 1):
                c_mk = _fact(m) / _fact(m - k)
                temp += (
                    e_neg_n
                    * (a**k * c_mk)
                    * (1j * omega_p) ** (-(k + 1))
                    * (b ** (m - k) - np.exp(-1j * omega_p) * (a + b) ** (m - k))
                )
            col = n * (M + 1) + m
            out[:, col] = temp

    return out


def _der_trans_mat(M: int, i: int, a: float, b: float) -> np.ndarray:
    """Derivative transformation matrix Δ^(i), shape (M+1, M+1).

    Equation (3.51) / function der_trans_mat of Hunter 2009.
    """
    vals = np.zeros(M + 1)
    if i <= M:
        vals[i:] = a**i * np.array([_fact(m) / _fact(m - i) for m in range(i, M + 1)])
    return np.roll(np.diag(vals), -i, axis=1)


def _mu_matrix_even(
    mu: float, M: int, N: int, a: float, b: float, i: int
) -> np.ndarray:
    """M_E^(i)(μ) for even-length PBFs, shape (N//2, N//2*(M+1)).

    Equation (3.74) of Hunter 2009.
    """
    half = N // 2
    Delta_i = _der_trans_mat(M, i, a, b)
    muab = a * mu + b
    pows = muab ** np.arange(M + 1)  # (M+1,)
    row0 = Delta_i @ pows  # (M+1,)

    mu_T = np.concatenate([row0, np.zeros((half - 1) * (M + 1))])  # (half*(M+1),)
    M_muE = np.tile(mu_T, (half, 1))  # (half, half*(M+1))

    for n in range(half):
        M_muE[n, :] = np.roll(M_muE[n, :], n * (M + 1))

    return M_muE


def _cE_to_C(cE: np.ndarray, N: int, M: int) -> np.ndarray:
    """Even coefficient vector → full C matrix, shape (N, M+1).

    Exploits linear-phase symmetry: C[N-1-n, m] = (-1)^m * C[n, m].
    Equation (3.68) of Hunter 2009.
    """
    half = N // 2
    # MATLAB reshape is column-major (Fortran order)
    half_C = np.reshape(cE, (M + 1, half), order="F").T  # (half, M+1)
    mscale = np.tile((-1) ** np.arange(M + 1), (half, 1))  # (half, M+1)
    return np.vstack([half_C, mscale * np.flipud(half_C)])  # (N, M+1)


def _C_to_c(C: np.ndarray) -> np.ndarray:
    """Full C matrix → coefficient vector, length N*(M+1)."""
    return C.flatten()  # row-major = reshape(C', N*(M+1)) in MATLAB


def _cont_eq_matrices(
    N: int, M: int, a: float, b: float, num_der: int
) -> tuple[Optional[np.ndarray], Optional[np.ndarray]]:
    """Equality constraint matrices for impulse-response continuity.

    Parameters
    ----------
    num_der:
        -1  → no constraints
         0  → impulse response continuous
         k  → impulse response + k derivatives continuous
    """
    if num_der < 0:
        return None, None

    half = N // 2

    # Fixed structural matrices
    P_mat = np.eye(half)
    P_mat[:, -1] = 0.0  # zero last column
    Q_mat = np.roll(P_mat, 1, axis=1)  # shift one col right
    R_mat = np.zeros((half, half))
    R_mat[0, 0] = 1.0

    b1 = np.zeros(half)
    b2 = np.zeros(half)
    b3 = np.zeros(half)

    Aeq_list = []
    beq_list = []

    for i in range(num_der + 1):
        M0 = _mu_matrix_even(0, M, N, a, b, i)  # (half, half*(M+1))
        M1 = _mu_matrix_even(1, M, N, a, b, i)

        A1 = P_mat @ M1 - Q_mat @ M0  # (half, half*(M+1))
        A2 = R_mat @ M0  # (half, half*(M+1))
        A = np.vstack([A1, A2])  # (N, half*(M+1))

        n_rows = N
        Aeqi = np.hstack([A, np.zeros((n_rows, 1))])  # (N, half*(M+1)+1)
        beqi = np.concatenate([b1, b2])  # (N,)

        if i % 2 != 0:
            S_mat = np.zeros((half, half))
            S_mat[-1, -1] = 1.0
            A3 = S_mat @ M1  # (half, half*(M+1))
            A = np.vstack([A, A3])  # (3*half, half*(M+1))
            n_rows = 3 * half
            Aeqi = np.hstack([A, np.zeros((n_rows, 1))])
            beqi = np.concatenate([beqi, b3])

        Aeq_list.append(Aeqi)
        beq_list.append(beqi)

    Aeq = np.vstack(Aeq_list)
    beq = np.concatenate(beq_list)
    return Aeq, beq


# ------------------------------------------------------------------ #
# Public: frequency / impulse response evaluation                     #
# ------------------------------------------------------------------ #


def pbf_freq_resp(C: np.ndarray, omega_p: np.ndarray, a: float, b: float) -> np.ndarray:
    """Frequency response of the PBF at the given radian frequencies.

    Parameters
    ----------
    C : np.ndarray
        Coefficient matrix, shape (N, M+1).
    omega_p : np.ndarray
        Radian frequencies (normalised, so 2π = one input sample period).
    a, b : float
        Basis function constants.

    Returns
    -------
    np.ndarray
        Complex frequency response, shape (len(omega_p),).
    """
    N, Mp1 = C.shape
    M = Mp1 - 1
    c = _C_to_c(C)
    Psi = _psi_hat_omega(M, N, omega_p, a, b)  # (P, N*(M+1))
    return Psi @ c


def pbf_imp_resp(
    C: np.ndarray,
    mu_p: np.ndarray,
    a: float,
    b: float,
    i_der: int = 0,
) -> tuple[np.ndarray, np.ndarray]:
    """Impulse response (or its i-th derivative) of the PBF.

    Returns
    -------
    n_mu : np.ndarray
        Normalised time axis (n + μ)·T with T=1.
    ha_n_mu : np.ndarray
        Impulse response values (real).
    """
    N, Mp1 = C.shape
    M = Mp1 - 1
    P = len(mu_p)
    muab = a * np.asarray(mu_p) + b  # (P,)

    # Build Vandermonde matrix: col p = muab[p]^(0:M)
    mu_mat = muab[np.newaxis, :] ** np.arange(M + 1)[:, np.newaxis]
    # mu_mat shape: (M+1, P)

    Delta_i = _der_trans_mat(M, i_der, a, b)  # (M+1, M+1)
    Hn = C @ Delta_i @ mu_mat  # (N, P)

    ha_n_mu = Hn.flatten()  # row-major

    n_grid = np.tile(mu_p, (N, 1)) + np.tile(
        np.arange(N)[:, np.newaxis], (1, P)
    )  # (N, P)
    n_mu = n_grid.flatten()

    return n_mu, ha_n_mu.real


# ------------------------------------------------------------------ #
# Public: LP optimization                                             #
# ------------------------------------------------------------------ #


def optimize_pbf(
    N: int,
    M: int,
    passband: float = 0.2,
    stopband: float = 0.8,
    a: float = 1.0,
    b: float = -0.5,
    K_pass: float = 10.0,
    K_stop: float = 1.0,
    n_pass: int = 100,
    n_stop: int = 500,
    num_der: int = -1,
) -> tuple[np.ndarray, float]:
    """Minimax PBF optimization via linear programming.

    Minimizes the weighted Chebyshev error between the PBF frequency
    response and the ideal lowpass response, subject to optional
    time-domain continuity constraints.

    Parameters
    ----------
    N : int
        Number of polynomial pieces (must be even for symmetry).
    M : int
        Polynomial order (e.g. 3 for cubic).
    passband : float
        Normalised passband edge (fraction of 2π).
    stopband : float
        Normalised stopband edge.
    a, b : float
        Basis function constants.  DPMFS default: a=1, b=-0.5.
    K_pass, K_stop : float
        Passband / stopband weights.
    n_pass, n_stop : int
        Number of frequency grid points in each band.
    num_der : int
        Continuity order (-1 = none, 0 = continuous, k = k derivatives
        continuous).

    Returns
    -------
    C : np.ndarray
        Coefficient matrix, shape (N, M+1).
    delta : float
        Minimised maximum weighted error.
    """
    omega_pass = 2 * np.pi * passband
    omega_stop = 2 * np.pi * stopband

    pb_pts = np.linspace(2 * np.pi * 0.01, omega_pass, n_pass)
    sb_pts = np.linspace(omega_stop, 2 * np.pi * 4.0, n_stop)
    omega = np.concatenate([pb_pts, sb_pts])  # (P,)
    _ = len(omega)

    W = K_pass * (omega <= omega_pass) + K_stop * (omega >= omega_stop)
    w_hat = (1.0 / W)[:, np.newaxis]  # (P, 1)
    d = (omega <= omega_pass).astype(float)  # (P,)

    # Even basis matrix (exploit linear-phase symmetry)
    n_coeff = (N // 2) * (M + 1)
    Psi_E = _psi_hat_even(M, N, omega, a, b)  # (P, n_coeff)

    # LP: min g'x  s.t.  A_ub x <= b_ub
    # x = [cE (n_coeff,); delta (1,)]
    A_ub = np.block(
        [
            [Psi_E, -w_hat],
            [-Psi_E, -w_hat],
        ]
    )  # (2P, n_coeff+1)
    b_ub = np.concatenate([d, -d])  # (2P,)
    g = np.concatenate([np.zeros(n_coeff), [1.0]])  # (n_coeff+1,)

    Aeq, beq = _cont_eq_matrices(N, M, a, b, num_der)

    if _linprog is None:
        raise ImportError(
            "scipy is required for LP optimization. "
            "Install it with: pip install 'doppler-dsp[optimize]'"
        )
    # cE coefficients are unbounded (can be negative); only delta >= 0
    bounds = [(None, None)] * n_coeff + [(0, None)]

    result = _linprog(
        g,
        A_ub=A_ub,
        b_ub=b_ub,
        A_eq=Aeq,
        b_eq=beq,
        bounds=bounds,
        method="highs",
        options={"disp": False},
    )

    if not result.success:
        raise RuntimeError(f"LP failed: {result.message}")

    cE = result.x[:-1]
    delta = float(result.x[-1])
    C = _cE_to_C(cE, N, M)

    return C, delta


def optimize_dpmfs(
    passband: float = 0.4,
    stopband: float = 0.6,
    attenuation: float = 60.0,
    N: int = 6,
    M: int = 3,
    a: float = 1.0,
    b: float = -0.5,
    num_der: int = -1,
    K_pass: float = 10.0,
    K_stop: float = 1.0,
    n_pass: int = 100,
    n_stop: int = 500,
) -> DPMFSCoeffs:
    """Design a DPMFS resampler via LP minimax optimization.

    Wraps :func:`optimize_pbf` and converts the result to a
    :class:`DPMFSCoeffs` object compatible with the C runtime.

    The polyphase decomposition uses J=2 (DPMFS):
        c[j, m, k] = C[2k + j, m]

    Parameters
    ----------
    passband, stopband : float
        Normalised band edges (fraction of 2π, i.e. fraction of Fin).
    attenuation : float
        Target stopband attenuation in dB (stored for reference only;
        the LP optimizer minimises the Chebyshev error directly).
    N : int
        Number of polynomial pieces (even).  Default 6.
    M : int
        Polynomial order.  Default 3 (cubic DPMFS).
    a, b : float
        Basis function constants.  Default a=1, b=-0.5.
    num_der : int
        Continuity order (-1 = unconstrained).
    K_pass, K_stop : float
        Passband / stopband LP weights.
    n_pass, n_stop : int
        Frequency grid density.

    Returns
    -------
    DPMFSCoeffs
        Fitted coefficient bank, shape c[2, M+1, N//2], float32.
    """
    C, delta = optimize_pbf(
        N=N,
        M=M,
        passband=passband,
        stopband=stopband,
        a=a,
        b=b,
        K_pass=K_pass,
        K_stop=K_stop,
        n_pass=n_pass,
        n_stop=n_stop,
        num_der=num_der,
    )

    # Convert basis: (aμ+b)^m → monomial μ^m via polynomial composition.
    # h(μ) = Σ_m C[n,m]·(aμ+b)^m  →  expand each (aμ+b)^m.
    half = N // 2
    c = np.zeros((2, M + 1, half), dtype=np.float64)

    for n in range(N):
        j = n % 2  # polyphase component
        k = n // 2  # tap index within branch
        if k >= half:
            continue
        # The LP polynomial (in original μ ∈ [0,1)):
        #   h_n(μ) = Σ_m C[n,m] · (a·μ + b)^m
        #
        # The DPMFS maps μ to μ_J ∈ [0,1) per branch:
        #   j=0: μ = μ_J/2          so  a·μ+b = (a/2)·μ_J + b
        #   j=1: μ = (μ_J + 1)/2    so  a·μ+b = (a/2)·μ_J + (a/2 + b)
        #
        # Build polynomial in μ_J: Σ_m C[n,m]·((a/2)·μ_J + b_j)^m
        b_j = b if j == 0 else (a / 2.0 + b)
        linear = np.poly1d([a / 2.0, b_j])

        poly = np.poly1d([0.0])
        basis = np.poly1d([1.0])  # linear^0 = 1
        for m in range(M + 1):
            if m > 0:
                basis = basis * linear
            poly = poly + float(C[n, m]) * basis
        # poly.coeffs is highest-degree first; we want lowest first
        mono = poly.coeffs[::-1]  # length up to M+1
        # Pad or truncate to M+1 coefficients
        c_k = np.zeros(M + 1)
        L = min(len(mono), M + 1)
        c_k[:L] = mono[:L]
        c[j, :, k] = c_k

    return DPMFSCoeffs(
        c=c.astype(np.float32),
        M=M,
        N=half,
        passband=passband,
        stopband=stopband,
        attenuation=attenuation,
        residual_rms=float(delta),
    )
