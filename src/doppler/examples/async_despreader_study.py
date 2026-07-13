"""async_despreader_study.py — symbol/code two-clock async despreading.

A design-validation study for the case where the **data-symbol rate is on
the order of the code-epoch rate but asynchronous** to it:
``Tsym = TE*(1+delta)`` with an independent phase. Coherent integrate-and-dump
over one *code* epoch then straddles data transitions (the symbol is on a
*different* clock), and the despread collapses.

It demonstrates, on a carrier-free code-aligned DSSS signal (the despreader's
input after carrier wipe-off and code lock):

  * **The failure** — per-epoch coherent despread: a data flip at fraction
    ``f`` into the epoch gives ``|P| = A*|2f-1|`` (zero at mid-epoch), and
    ``delta`` sweeps ``f`` through every value, so the BER **floors**
    regardless of Es/N0. The ``|P[n]|`` spectrum shows a tone at ``|delta|``
    cyc/epoch — the fingerprint that identifies this failure class.

  * **The fix** — ``K`` sub-epoch **partial correlations** per epoch (the
    symbol clock becomes observable at ``K`` samples/symbol) -> a length-``K``
    **boxcar symbol matched filter** (a *sliding*, symbol-aligned coherent
    re-integration, not epoch-locked) -> :class:`doppler.track.SymbolSync`
    (Gardner + Farrow) recovers the independent symbol clock. BER follows the
    BPSK matched-filter bound within ~1-2 dB; a genie reference with known
    symbol timing hits it exactly (the loss was only window misalignment).

  * **Code tracking under async data** — the DLL's early/late discriminator
    collapses on straddle epochs (``|E|,|L| -> 0``). **Non-coherent** combining
    of the partials (``sum_k |E_k|``) is robust: a data flip changes a
    partial's *sign*, not its magnitude, so only the one straddling segment
    degrades. It roughly halves the discriminator variance (``K=4`` sweet
    spot; larger ``K`` loses gain to the Rician/squaring bias).

Run:  python -m doppler.examples.async_despreader_study  [out.png]
"""

from __future__ import annotations

import sys
from math import erfc, sqrt

import numpy as np

from doppler.track import SymbolSync
from doppler.wfm import PN, mls_poly

SPS, NMLS = 8, 7
SF = (1 << NMLS) - 1  # 127 chips
TE = SF * SPS  # 1016 samples per code epoch
_CS = np.where(
    np.asarray(
        PN(poly=mls_poly(NMLS), seed=1, length=NMLS).generate(SF)
    ).astype(np.uint8)
    & 1,
    -1.0,
    1.0,
)
CHIP = np.repeat(_CS, SPS)  # one epoch of the code waveform
SP = SPS // 2  # half-chip early/late spacing (samples)
EARLY, LATE = np.roll(CHIP, -SP), np.roll(CHIP, SP)


def _sigma2(esn0):
    """Noise variance for a target per-symbol matched-filter Es/N0 (dB)."""
    return SF * SPS / (2.0 * 10 ** (esn0 / 10))


def gen(nsym, delta, phi, esn0, seed, tau0=0.0):
    """Carrier-free, code-aligned DSSS; data on an independent symbol clock.

    ``Tsym = TE*(1+delta)`` samples per symbol, phase ``phi`` samples; ``tau0``
    advances the signal's code phase (chips) to exercise the DLL discriminator.
    """
    rng = np.random.default_rng(seed)
    tsym = TE * (1.0 + delta)
    n_samp = int(nsym * tsym) + 2 * TE
    data = (rng.integers(0, 2, nsym + 6) * 2 - 1).astype(float)
    n = np.arange(n_samp)
    si = np.clip(np.floor((n - phi) / tsym).astype(int), 0, len(data) - 1)
    code = np.roll(CHIP, round(tau0 * SPS))
    rx = data[si] * code[n % TE] + rng.normal(0, sqrt(_sigma2(esn0)), n_samp)
    return rx.astype(np.complex64), data, tsym


def epoch_despread(rx):
    """Broken baseline: coherent integrate-and-dump over each code epoch."""
    nep = len(rx) // TE
    return (rx[: nep * TE].real.reshape(nep, TE) * CHIP).sum(1) / TE


def partial_despread(rx, k):
    """K sub-epoch partial prompt correlations -> ~K samples per symbol."""
    nep, seg = len(rx) // TE, TE // k
    blk = (rx[: nep * TE].real.reshape(nep, TE) * CHIP).reshape(nep, k, seg)
    return (blk.sum(2) / seg).reshape(-1).astype(np.complex64)


def recover(rx, k=4):
    """Robust receiver: partial despread -> boxcar symbol MF -> SymbolSync."""
    pp = partial_despread(rx, k).real
    mf = np.convolve(pp, np.ones(k), "same").astype(np.complex64)
    ss = SymbolSync(sps=k, bn=0.01, zeta=0.707, order="cubic")
    return ss.steps(mf)


def genie(rx, data, tsym, phi, nsym):
    """Upper bound: coherent symbol-aligned despread with known timing.

    Vectorised via a prefix sum of ``rx*code``: each symbol's despread is a
    window difference, so there is no per-symbol Python loop.
    """
    nsamp = len(rx)
    w = rx.real * CHIP[np.arange(nsamp) % TE]
    pref = np.concatenate([[0.0], np.cumsum(w)])
    m = np.arange(2, nsym - 2)
    lo = np.clip(np.ceil(phi + m * tsym).astype(int), 0, nsamp)
    hi = np.clip((phi + (m + 1) * tsym).astype(int), 0, nsamp)
    return pref[hi] - pref[lo], data[2 : nsym - 2]


def ber(dec, data):
    """BER with best small-offset alignment and BPSK 180-deg sign ambiguity."""
    dec = np.sign(np.real(dec))
    dec[dec == 0] = 1
    best = 1.0
    for off in range(-4, 5):
        a, b = (dec[off:], data) if off >= 0 else (dec, data[-off:])
        length = min(len(a), len(b))
        if length < 100:
            continue
        e = np.mean(a[:length] != b[:length])
        best = min(best, e, 1 - e)
    return best


def disc_std(rx, k):
    """Early-late discriminator std: coherent-epoch vs non-coherent-partial."""
    nep, seg = len(rx) // TE, TE // k
    r = rx[: nep * TE].real.reshape(nep, TE)
    ek = (r.reshape(nep, k, seg) * EARLY.reshape(k, seg)).sum(2)
    lk = (r.reshape(nep, k, seg) * LATE.reshape(k, seg)).sum(2)
    ee, le = np.abs(ek.sum(1)), np.abs(lk.sum(1))  # sum partials, then |.|
    en, ln = np.abs(ek).sum(1), np.abs(lk).sum(1)  # |.| per partial, then sum
    coh = (ee - le) / (ee + le + 1e-12)
    nc = (en - ln) / (en + ln + 1e-12)
    return coh.std(), nc.std()


def main(out_path="async_despreader_study.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    esn0s = np.array([4, 6, 8, 9.6])
    th = 0.5 * np.array([erfc(sqrt(10 ** (e / 10))) for e in esn0s])
    broken, fixed, gen_ber = [], [], []
    for e in esn0s:
        rx, data, tsym = gen(15000, 3e-3, 0.37 * TE, e, 3)
        g, gd = genie(rx, data, tsym, 0.37 * TE, 15000)
        # broken: per-epoch coherent despread, each epoch mapped to the data
        # symbol covering its centre (this removes the trivial epoch<->symbol
        # desync, isolating the straddle-cancellation floor).
        pe = epoch_despread(rx)
        ctr = (np.arange(len(pe)) * TE + TE / 2 - 0.37 * TE) / tsym
        truth = data[np.clip(np.floor(ctr).astype(int), 0, len(data) - 1)]
        broken.append(ber(pe, truth))
        fixed.append(ber(recover(rx, 4), data))
        gen_ber.append(ber(g, gd))

    # |P| beat fingerprint (noiseless, to isolate the straddle modulation)
    rx, _, _ = gen(900, 3e-3, 0.37 * TE, 60, 1)
    mag = np.abs(epoch_despread(rx))
    spec = np.abs(np.fft.rfft((mag - mag.mean()) * np.hanning(len(mag))))
    freq = np.fft.rfftfreq(len(mag))

    # discriminator robustness vs K (static code offset, async data)
    ks = [2, 4, 8]  # must divide TE = 1016 = 8*127
    rxd, _, _ = gen(4000, 3e-3, 0.37 * TE, 10, 5, tau0=0.25)
    coh = [disc_std(rxd, k)[0] for k in ks]
    ncs = [disc_std(rxd, k)[1] for k in ks]

    # ── validate the three claims ────────────────────────────────────
    # 1. The per-epoch coherent despread floors (straddle cancellation
    #    kills symbols regardless of Es/N0) while the partial+MF+
    #    SymbolSync fix and the genie both ride the BPSK bound (~1e-5
    #    at 9.6 dB → expect ~0 errors in 15k symbols; allow a few).
    print(
        f"BER at {esn0s[-1]} dB: broken {broken[-1]:.1e}, "
        f"fixed {fixed[-1]:.1e}, genie {gen_ber[-1]:.1e}"
    )
    assert min(broken) > 1e-2, "broken baseline did not floor"
    assert fixed[-1] < 1e-3, "fix does not approach the BPSK bound"
    assert gen_ber[-1] < 1e-3, "genie despread off the BPSK bound"
    # 2. The failure fingerprint: the |P| fluctuation spectrum peaks at
    #    the symbol/code clock offset |delta| (within FFT resolution).
    fpk = float(freq[np.argmax(spec)])
    print(f"|P| beat tone at {fpk:.2e} cyc/epoch (|delta| = 3e-3)")
    assert abs(fpk - 3e-3) < 2.0 / len(mag), "beat tone not at |delta|"
    # 3. Non-coherent partial combining is the more robust code
    #    discriminator under async data (K=4 sweet spot).
    k4 = ks.index(4)
    print(
        f"disc std at K=4: coherent {coh[k4]:.3f}, non-coherent {ncs[k4]:.3f}"
    )
    assert ncs[k4] < coh[k4], "non-coherent partials not more robust"

    fig, ax = plt.subplots(1, 3, figsize=(15, 4.3))
    ax[0].semilogy(esn0s, th, "k--", lw=2, label="BPSK bound")
    ax[0].semilogy(esn0s, gen_ber, "s-", color="#1f77b4", label="genie timing")
    ax[0].semilogy(
        esn0s, fixed, "o-", color="#2ca02c", label="partial+MF+SymbolSync"
    )
    ax[0].semilogy(
        esn0s, broken, "x-", color="#d62728", label="coherent epoch (broken)"
    )
    ax[0].set_xlabel("Es/N0 (dB)")
    ax[0].set_ylabel("BER")
    ax[0].set_title("BER: broken floors, fix tracks the bound", fontsize=10)
    ax[0].legend(fontsize=8)
    ax[0].grid(alpha=0.3, which="both")

    ax[1].plot(freq, spec, color="#9467bd", lw=1.2)
    ax[1].axvline(3e-3, color="0.5", ls=":", label="|delta| = 3e-3")
    ax[1].set_xlim(0, 0.02)
    ax[1].set_xlabel("frequency (cycles / epoch)")
    ax[1].set_ylabel("|P| fluctuation spectrum")
    ax[1].set_title("Failure fingerprint: beat tone at |delta|", fontsize=10)
    ax[1].legend(fontsize=8)
    ax[1].grid(alpha=0.3)

    ax[2].plot(ks, coh, "x-", color="#d62728", label="coherent epoch")
    ax[2].plot(ks, ncs, "o-", color="#2ca02c", label="non-coherent partial")
    ax[2].set_xlabel("partials per epoch  K")
    ax[2].set_ylabel("discriminator std (lower = robust)")
    ax[2].set_title("DLL robustness under async data", fontsize=10)
    ax[2].legend(fontsize=8)
    ax[2].grid(alpha=0.3)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(
        f"wrote {out_path}  (broken BER floor ~{max(broken):.1e}, "
        f"fix at 9.6 dB = {fixed[-1]:.1e}, bound = {th[-1]:.1e})"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_despreader_study.png")
