# DLL Code Loop — Theory Validation

![DLL theory validation](../assets/dll_theory_demo.png)

A theoretical-correctness check on [`track.Dll`](../api/python-track.md)'s
non-coherent early-minus-late code discriminator `(|E|-|L|)/(|E|+|L|)`.

**Left — Code-detector S-curve.** The open-loop discriminator (swept static
code-phase error, bn → 0) follows the **triangular-autocorrelation E-L
reference** `(R(τ+s)-R(τ-s))/(R(τ+s)+R(τ-s))`, `R(τ)=max(0,1-|τ|)`: zero with a
restoring (negative) slope at the lock, linear within the early-late span, and
saturating beyond ±½ chip. The visible staircase is sub-chip code-phase
quantization at `sps=16`; its asymmetry **halves with each `sps` doubling**
(0.25 → 0.13 → 0.06), i.e. vanishes in the continuous limit.

**Right — Code-error variance vs SNR.** At the lock the early-late discriminator
variance follows a clean **`1/SNR`** law (the per-epoch code-error noise) — the
measurements lie on the line across four decades of SNR.

This completes the three tracking loops' theory validation: the
[Costas carrier loop](costas-theory.md), the [Gardner timing loop](symsync-theory.md),
and the DLL code loop.

Source: `src/doppler/examples/dll_theory_demo.py`;
tests in `src/doppler/track/tests/test_theory_dll.py`.
