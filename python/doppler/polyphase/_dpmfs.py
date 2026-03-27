"""
doppler.polyphase._dpmfs
========================

Dual Phase Modified Farrow Structure (DPMFS) coefficient design via
least-squares polynomial fitting.

Reference
---------
M. T. Hunter and W. B. Mikhael, "A Novel Farrow Structure with
Reduced Complexity," ICASSP 2009.

Design method
-------------
Given a dense polyphase bank of shape (L, N) from
``kaiser_prototype``, fit an M-th order polynomial per tap per
polyphase component (j=0, j=1):

    h_k(μ) ≈ Σ_m  c[j, m, k] · μ_J^m

where j = ⌊2μ⌋ and μ_J = 2μ − j.  Each half of the μ range
[0, 0.5) and [0.5, 1.0) maps to μ_J ∈ [0, 1) — the polynomial only
needs to cover half the original range, so a low-order fit is tight.

Runtime evaluation (per output sample)
---------------------------------------
    j    = 1 if μ >= 0.5 else 0
    μ_J  = 2μ − j
    h[k] = c[j, M, k]
    for m in range(M-1, -1, -1):
        h[k] = h[k] * μ_J + c[j, m, k]   # Horner
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

__all__ = ["DPMFSCoeffs", "fit_dpmfs"]


@dataclass
class DPMFSCoeffs:
    """DPMFS polynomial coefficient bank.

    ``c`` has shape ``(2, M+1, N)``, float32:

    * axis 0: j ∈ {0, 1}   — polyphase component (j = ⌊2μ⌋)
    * axis 1: m ∈ {0..M}   — polynomial order
    * axis 2: k ∈ {0..N-1} — tap index (inner, contiguous for C)

    At a given fractional delay μ ∈ [0, 1)::

        j    = int(2 * mu) & 1
        mu_J = 2 * mu - j
        h[k] = Horner(c[j, :, k], mu_J)
    """

    c: np.ndarray       # float32, shape (2, M+1, N)
    M: int              # polynomial order
    N: int              # taps per phase
    passband: float
    stopband: float
    attenuation: float
    residual_rms: float # RMS coefficient fit error

    # ------------------------------------------------------------------
    # Evaluation
    # ------------------------------------------------------------------

    def evaluate(self, mu: float) -> np.ndarray:
        """Return the N-tap coefficient vector for fractional delay μ.

        Parameters
        ----------
        mu : float
            Fractional delay in [0, 1).

        Returns
        -------
        np.ndarray
            float32 array of shape (N,).
        """
        mu = float(mu) % 1.0
        j = 1 if mu >= 0.5 else 0
        mu_J = 2.0 * mu - j
        h = self.c[j, self.M, :].astype(np.float64)
        for m in range(self.M - 1, -1, -1):
            h = h * mu_J + self.c[j, m, :]
        return h.astype(np.float32)

    # ------------------------------------------------------------------
    # Validation
    # ------------------------------------------------------------------

    def validate(
        self,
        polyphase: np.ndarray,
        verbose: bool = True,
    ) -> dict:
        """Compare reconstructed coefficients against a reference bank.

        Parameters
        ----------
        polyphase : np.ndarray
            Reference bank of shape (L, N) from ``kaiser_prototype``.
        verbose : bool
            Print a one-line summary when True.

        Returns
        -------
        dict
            ``rms`` and ``max_abs`` coefficient errors.
        """
        polyphase = np.asarray(polyphase, dtype=np.float64)
        L, N = polyphase.shape
        mu_vals = np.arange(L) / L
        h_approx = np.array(
            [self.evaluate(mu).astype(np.float64) for mu in mu_vals]
        )
        err = h_approx - polyphase
        rms = float(np.sqrt(np.mean(err ** 2)))
        max_abs = float(np.max(np.abs(err)))
        if verbose:
            print(
                f"  DPMFS fit  M={self.M}  N={self.N}"
                f"  rms={rms:.2e}  max={max_abs:.2e}"
            )
        return {"rms": rms, "max_abs": max_abs}

    # ------------------------------------------------------------------
    # Code generation
    # ------------------------------------------------------------------

    def to_c_header(self, name: str = "dpmfs") -> str:
        """Return a C header snippet with the coefficient arrays.

        The arrays are named ``{name}_c{j}_m{m}[N]``, suitable for
        inclusion in a C source file.
        """
        u = name.upper()
        lines = [
            f"/* DPMFS coefficients — M={self.M}, N={self.N} */",
            f"/* passband={self.passband}, stopband={self.stopband},"
            f" atten={self.attenuation} dB */",
            f"/* fit residual rms={self.residual_rms:.2e} */",
            f"#define {u}_M  {self.M}",
            f"#define {u}_N  {self.N}",
            "",
        ]
        for j in range(2):
            for m in range(self.M + 1):
                vals = ", ".join(
                    f"{v:.8e}f" for v in self.c[j, m, :]
                )
                lines.append(
                    f"static const float {name}_c{j}_m{m}"
                    f"[{self.N}] = {{{vals}}};"
                )
            lines.append("")
        return "\n".join(lines)


# ----------------------------------------------------------------------
# Design
# ----------------------------------------------------------------------

def fit_dpmfs(
    polyphase: np.ndarray,
    M: int = 3,
    passband: float = 0.4,
    stopband: float = 0.6,
    attenuation: float = 60.0,
) -> DPMFSCoeffs:
    """Fit DPMFS coefficients to a polyphase bank via least squares.

    For each polyphase component j ∈ {0, 1} and each tap k, solves::

        V @ c[j, :, k] ≈ polyphase[j*half : (j+1)*half, k]

    where V is a Vandermonde matrix in μ_J ∈ [0, 1).

    Parameters
    ----------
    polyphase : np.ndarray
        Float32 array of shape (L, N) — the ``polyphase`` output of
        :func:`kaiser_prototype`.  L must be a power of two ≥ 2.
    M : int
        Polynomial order (default 3).  Order 3 (cubic) gives tight
        fits for smooth prototype filters with minimal overshoot.
    passband, stopband, attenuation : float
        Filter specifications stored in the result for reference.
        These do not affect the fit — they describe the input bank.

    Returns
    -------
    DPMFSCoeffs
        Polynomial coefficient bank ``c[j, m, k]``, float32.
    """
    polyphase = np.asarray(polyphase, dtype=np.float64)
    L, N = polyphase.shape
    if L < 2 or (L & (L - 1)):
        raise ValueError(f"L must be a power of two >= 2, got {L}")
    if M < 1:
        raise ValueError(f"M must be >= 1, got {M}")
    half = L // 2

    # μ_J ∈ [0, 1) — same grid for both j=0 and j=1
    # For j=0: μ ∈ [0, 0.5) → μ_J = 2μ ∈ [0, 1)
    # For j=1: μ ∈ [0.5, 1) → μ_J = 2μ-1 ∈ [0, 1)
    # Both halves map identically to μ_J ∈ [0, 1).
    mu_J = np.arange(half) / half              # (half,)
    V = np.vander(mu_J, M + 1, increasing=True)  # (half, M+1)

    c = np.zeros((2, M + 1, N), dtype=np.float64)
    total_sq_err = 0.0

    for j in range(2):
        rows = polyphase[j * half : (j + 1) * half, :]  # (half, N)
        # Least-squares: min ||V @ C - rows||_F
        # C has shape (M+1, N); each column is one tap's polynomial.
        coeffs, _, _, _ = np.linalg.lstsq(V, rows, rcond=None)
        c[j] = coeffs
        residuals = V @ coeffs - rows
        total_sq_err += float(np.sum(residuals ** 2))

    rms = float(np.sqrt(total_sq_err / (L * N)))

    return DPMFSCoeffs(
        c=c.astype(np.float32),
        M=M,
        N=N,
        passband=passband,
        stopband=stopband,
        attenuation=attenuation,
        residual_rms=rms,
    )
