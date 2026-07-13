"""async_despread_demo.py — the streaming code despreader: PN in, oversampled
asynchronous BPSK out.

Drives :class:`doppler.track.Dll` in **segments** mode — the streaming DSSS
despreader. Its one job is to *remove the PN code and output samples*. The data
symbols ride on a clock that is **asynchronous** to the code epoch; that is
merely why it despreads in ``K`` sub-epoch **partial** correlations, so a
mid-epoch data flip cannot collapse the non-coherent code discriminator. A
small **residual carrier** rides out on the output — carrier recovery and
symbol-timing recovery are *downstream*, not this object's job.

Writes TWO figures. The despread story is noiseless so the envelope is clean;
the lock detector needs noise to be meaningful, so it gets its own figure on
its own noisy signal — the two are never conflated.

``<out>.png`` — the despreader, three noiseless views:
  * **Oversampled async BPSK out** — the despread partial stream at `K`
    samples/symbol; the symbol edges (dashed) slide through the code epochs
    because the symbol clock is independent of the code clock.
  * **Carrier rides on the output** — the same partials in the complex plane:
    two BPSK clusters smeared onto a ring by the residual carrier; a downstream
    carrier loop collapses them back to ±1.
  * **Code stays locked under the carrier** — the non-coherent ``(|E|-|L|)``
    discriminator is carrier-blind, so the code rate tracks the code Doppler
    with the residual carrier still on the samples.

``<out>_lock.png`` — the always-on lock detector on a SEPARATE noisy run: the
  non-coherent lock statistic ``R = sqrt(2*sum|P|^2 / E|O|^2)`` (acquisition's
  test, with a random off-peak EMA noise reference) climbing past the CFAR
  threshold at several SNRs, beside the noisy despread output it ran on.

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
NSYM = 1200


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

    # The non-coherent (|E|-|L|) discriminator is carrier-blind: the
    # settled code-rate estimate must sit on the true code Doppler with
    # the residual carrier still on the samples.
    rate_err = float(np.abs(rate[nep // 2 :].mean() - (1.0 + DCODE)))
    print(f"settled code-rate err = {rate_err:.1e} (Doppler = {DCODE:.0e})")
    assert rate_err < 0.25 * DCODE, "DLL settled off the true code rate"

    # Noiseless despread: with the code wiped, partials sit at the unit
    # signal amplitude except the 1-in-K straddling each async symbol
    # edge — the settled envelope median must be ~1.
    env_med = float(np.median(np.abs(part[len(part) // 2 :])))
    print(f"settled despread envelope median = {env_med:.3f}")
    assert env_med > 0.9, "despread envelope collapsed"

    # Downstream carrier wipe (genie): de-rotate each partial by the residual
    # carrier at its centre sample, then align the BPSK axis to the real axis
    # (this is the job of a downstream Costas loop — shown here for clarity).
    tc = TE * (np.arange(len(part)) + 0.5) / K
    dero = part * np.exp(-2j * np.pi * F0 * tc)

    fig, (a, b, c) = plt.subplots(1, 3, figsize=(13.5, 4.2))

    # --- 1. oversampled asynchronous BPSK out (settled window) ---
    off = (len(part) * 6 // 10) // K * K  # past the code-loop pull-in
    nshow = 16
    span = nshow * K
    pp = np.arange(off, off + span)
    sl = slice(off, off + span)
    # align the BPSK to the real axis within this window (a downstream Costas
    # job; genie here): residual carrier ≈ a constant phase over the window
    win = dero[sl] * np.exp(-0.5j * np.angle(np.mean(dero[sl] ** 2)))
    a.plot(
        pp,
        win.real,
        "-o",
        color="#1f77b4",
        ms=3.5,
        lw=1.0,
        label="despread (real)",
    )
    # the despread ENVELOPE makes the amplitude modulation explicit
    a.plot(pp, np.abs(part[sl]), color="#999999", lw=0.9, label="|despread|")
    a.plot(pp, -np.abs(part[sl]), color="#999999", lw=0.9)
    a.axhline(0, color="k", lw=0.6)
    m = 0  # async symbol edges (partials come out at the tracked code rate)
    while True:
        b_pp = (PHI + m * tsym) * K * (1.0 + DCODE) / TE
        if b_pp > off + span:
            break
        if b_pp >= off:
            a.axvline(b_pp, color="#d62728", ls="--", lw=1.0, alpha=0.7)
        m += 1
    a.set_xlim(off, off + span)
    a.set_title(
        f"Oversampled async BPSK out — {K}/symbol\n"
        "envelope dips: the 1/K straddle at each async symbol edge (red)",
        fontsize=9,
    )
    a.set_xlabel("partial index (= output sample)")
    a.set_ylabel("despread output (real)")
    a.legend(fontsize=7, loc="lower right")
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

    # The lock detector is a SEPARATE experiment — it needs noise to be
    # meaningful, whereas the panels above are noiseless so the envelope story
    # stays clean. Draw it in its own figure (on its own noisy signal) so the
    # two are never conflated.
    lock_path = (
        out_path[:-4] + "_lock.png"
        if out_path.endswith(".png")
        else out_path + "_lock.png"
    )
    _lock_figure(code, plt, lock_path)


def _lock_figure(code, plt, out_path):
    """Always-on lock detector on its OWN noisy run (distinct from main()).

    Two panels: the lock statistic ``R`` climbing past the CFAR threshold at a
    few per-sample SNRs (with a noise-only trace that stays below), and the
    noisy despread output that produced the middle trace — so it is clearly a
    *different, noisy* signal from the noiseless despread figure.
    """
    from doppler.detection import det_threshold_noncoherent

    rx, _ = _signal(code)
    nep = len(rx) // TE
    thr = det_threshold_noncoherent(1e-3, 20)  # default lock config
    rng = np.random.default_rng(5)

    def add_noise(namp):
        z = rng.standard_normal(len(rx)) + 1j * rng.standard_normal(len(rx))
        return (rx + namp * z / np.sqrt(2)).astype(np.complex64)

    def noise_only(namp):
        z = rng.standard_normal(len(rx)) + 1j * rng.standard_normal(len(rx))
        return (namp * z / np.sqrt(2)).astype(np.complex64)

    fig, (a, b) = plt.subplots(1, 2, figsize=(11.5, 4.4))

    # --- left: R vs epoch at several SNRs, + noise-only, vs threshold ---
    runs = [
        ("strong (namp 3)", add_noise(3.0), "#1f77b4", None),
        ("weak (namp 9)", add_noise(9.0), "#2ca02c", "mid"),
        ("very weak (namp 16)", add_noise(16.0), "#ff7f0e", None),
        ("noise only", noise_only(9.0), "#d62728", None),
    ]
    mid_part = None
    for label, rxn, col, tag in runs:
        dl = Dll(code, SPS, 0.0, 0.002, 0.707, 0.5, segments=K)
        R = np.empty(nep)
        chunks = []
        for e in range(nep):
            out = dl.steps(rxn[e * TE : (e + 1) * TE])
            R[e] = dl.lock_stat
            if tag == "mid":
                chunks.append(out)
        a.plot(np.arange(nep), R, color=col, lw=1.0, label=label)
        if tag == "mid":
            mid_part = np.concatenate(chunks)
        # CFAR behaviour: the strong and weak signal runs' settled lock
        # statistic must sit above the threshold; the noise-only run
        # must not.  The "very weak" run is deliberately marginal (it
        # hovers at the gate) and is shown, not asserted.
        r_med = float(np.median(R[nep // 2 :]))
        print(f"lock stat median ({label}) = {r_med:.1f} vs eta {thr:.1f}")
        if label == "noise only":
            assert r_med < thr, "noise-only lock stat crossed the gate"
        elif not label.startswith("very weak"):
            assert r_med > thr, f"lock not declared on the {label} run"
    a.axhline(thr, color="k", ls="--", lw=1.3, label=f"η={thr:.1f} (pfa=1e-3)")
    a.set_ylim(0, None)
    a.set_title(
        "Lock statistic R vs epoch (noisy)\n"
        "R = √(2·Σ|P|²/E|O|²); SF=127 → signal sits far above η",
        fontsize=9,
    )
    a.set_xlabel("code epoch")
    a.set_ylabel("lock statistic R")
    a.legend(fontsize=7, loc="center right")
    a.grid(alpha=0.25)

    # --- right: the noisy despread output behind the "weak" trace ---
    off = (len(mid_part) * 6 // 10) // K * K
    span = 24 * K
    pp = np.arange(off, off + span)
    sl = slice(off, off + span)
    a2 = mid_part[sl]
    a2 = a2 * np.exp(-0.5j * np.angle(np.mean(a2**2)))  # genie axis align
    b.plot(
        pp,
        a2.real,
        "-o",
        color="#2ca02c",
        ms=3,
        lw=0.9,
        label="despread (real)",
    )
    b.plot(
        pp, np.abs(mid_part[sl]), color="#999999", lw=0.8, label="|despread|"
    )
    b.axhline(0, color="k", lw=0.6)
    b.set_xlim(off, off + span)
    b.set_title(
        "Noisy despread output (the 'weak' run)\n"
        "BPSK still recoverable; the detector reports lock on it",
        fontsize=9,
    )
    b.set_xlabel("partial index (= output sample)")
    b.set_ylabel("despread output")
    b.legend(fontsize=7, loc="lower right")
    b.grid(alpha=0.25)

    fig.suptitle(
        "track.Dll always-on code-lock detector "
        "(separate noisy run — not the noiseless figure)",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_despread_demo.png")
