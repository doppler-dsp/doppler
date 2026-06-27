"""async_despread_demo.py — the streaming code despreader: PN in, oversampled
asynchronous BPSK out.

Drives :class:`doppler.track.Dll` in **segments** mode — the streaming DSSS
despreader. Its one job is to *remove the PN code and output samples*. The data
symbols ride on a clock that is **asynchronous** to the code epoch; that is
merely why it despreads in ``K`` sub-epoch **partial** correlations, so a
mid-epoch data flip cannot collapse the non-coherent code discriminator. A
small **residual carrier** rides out on the output — carrier recovery and
symbol-timing recovery are *downstream*, not this object's job.

Three views (saved to a PNG):
  * **Oversampled async BPSK out** — the despread partial stream at `K`
    samples/symbol; the symbol edges (dashed) slide through the code epochs
    because the symbol clock is independent of the code clock.
  * **Carrier rides on the output** — the same partials in the complex plane:
    two BPSK clusters smeared onto a ring by the residual carrier; a downstream
    carrier loop collapses them back to ±1.
  * **Code stays locked under the carrier** — the non-coherent ``(|E|-|L|)``
    discriminator is carrier-blind, so the code rate tracks the code Doppler
    with the residual carrier still on the samples.

Run:  python -m doppler.examples.async_despread_demo  [out.png]
"""

from __future__ import annotations

import sys

import numpy as np

from doppler.track import Dll

SF, SPS, K = 127, 8, 4  # code chips, samples/chip, partials/epoch
TE = SF * SPS  # code-epoch length, samples
F0 = 3e-4  # residual carrier, cyc/sample (< ½ epoch-FFT bin = 1/2TE)
DCODE = 2e-4  # code Doppler (chip-rate offset)
DSYM = 4e-3  # symbol-vs-code rate offset (async)
PHI = 0.37 * TE  # symbol-clock phase, samples
NSYM = 400


def _signal(code, seed=9):
    """Async-data PN-spread BPSK with a residual carrier left on it."""
    rng = np.random.default_rng(seed)
    csign = np.where(code & 1, -1.0, 1.0)
    tsym = TE * (1.0 + DSYM)
    n = int(NSYM * tsym) + 2 * TE
    idx = np.arange(n)
    data = (rng.integers(0, 2, NSYM + 6) * 2 - 1).astype(float)
    si = np.clip(np.floor((idx - PHI) / tsym).astype(int), 0, len(data) - 1)
    cph = (idx * (1.0 + DCODE) / SPS).astype(int) % SF
    rx = data[si] * csign[cph] * np.exp(2j * np.pi * F0 * idx)
    return rx.astype(np.complex64), tsym


def main(out_path="async_despread_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    code = np.random.default_rng(11).integers(0, 2, SF).astype(np.uint8)
    rx, tsym = _signal(code)
    d = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=K)

    nep = len(rx) // TE
    rate = np.empty(nep)
    chunks = []
    for e in range(nep):
        chunks.append(d.steps(rx[e * TE : (e + 1) * TE]))
        rate[e] = d.code_rate
    part = np.concatenate(chunks)  # K partials per epoch ~ K samples/symbol

    # Downstream carrier wipe (genie): de-rotate each partial by the residual
    # carrier at its centre sample, then align the BPSK axis to the real axis
    # (this is the job of a downstream Costas loop — shown here for clarity).
    tc = TE * (np.arange(len(part)) + 0.5) / K
    dero = part * np.exp(-2j * np.pi * F0 * tc)
    dero *= np.exp(-0.5j * np.angle(np.mean(dero**2)))

    fig, (a, b, c) = plt.subplots(1, 3, figsize=(13.5, 4.2))

    # --- 1. oversampled asynchronous BPSK out ---
    nshow = 14  # symbols to zoom
    npp = nshow * K + K
    pp = np.arange(npp)
    a.plot(pp, dero.real[:npp], "-o", color="#1f77b4", ms=3.5, lw=1.0)
    a.axhline(0, color="k", lw=0.6)
    # async symbol boundaries (in partial-index units), sliding vs the epochs
    m = 0
    while True:
        b_pp = (PHI + m * tsym) * K / TE
        if b_pp > npp:
            break
        if b_pp >= 0:
            a.axvline(b_pp, color="#d62728", ls="--", lw=1.0, alpha=0.7)
        m += 1
    for e in range(npp // K + 1):  # code-epoch ticks (the despreader's clock)
        a.axvline(e * K, color="gray", ls=":", lw=0.6, alpha=0.5)
    a.set_title(
        f"Oversampled async BPSK out — {K} partials/symbol\n"
        "red: symbol edges (async)   gray: code epochs",
        fontsize=9,
    )
    a.set_xlabel("partial index (= output sample)")
    a.set_ylabel("despread output (real)")
    a.grid(alpha=0.25)

    # --- 2. carrier rides on the output (raw partials) ---
    w = part[200 : 200 + 80 * K]
    c0 = c  # keep name; plot constellation on axis b
    b.scatter(w.real, w.imag, s=12, color="#7f7f7f", alpha=0.5)
    lim = 1.2 * np.max(np.abs(w))
    b.set_xlim(-lim, lim)
    b.set_ylim(-lim, lim)
    b.set_aspect("equal")
    b.axhline(0, color="k", lw=0.5)
    b.axvline(0, color="k", lw=0.5)
    b.set_title(
        "Carrier rides on the output\n(2 BPSK clusters smeared to a ring)",
        fontsize=9,
    )
    b.set_xlabel("I")
    b.set_ylabel("Q")
    b.grid(alpha=0.25)

    # --- 3. code stays locked under the carrier ---
    c0.axhline(1.0 + DCODE, color="k", ls="--", lw=1.3, label="true code rate")
    c0.plot(
        np.arange(nep), rate, color="#2ca02c", lw=1.1, label="DLL estimate"
    )
    c0.set_title(
        "Code stays locked under the carrier\n(non-coherent loop is "
        "carrier-blind)",
        fontsize=9,
    )
    c0.set_xlabel("code epoch")
    c0.set_ylabel("code rate (chips/chip)")
    c0.legend(fontsize=8, loc="lower right")
    c0.grid(alpha=0.25)

    fig.suptitle(
        f"track.Dll(segments={K}) streaming despreader — remove the PN, "
        f"output samples (SF={SF}, sps={SPS}, residual carrier {F0:.0e}, "
        f"async data δ={DSYM:.0e})",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_despread_demo.png")
