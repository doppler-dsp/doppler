"""wfm_rrc_response.py — RRC impulse / frequency response and Nyquist no-ISI.

Visualises the root-raised-cosine pulse-shaping taps from
:func:`doppler.wfm.rrc_taps` (``native/src/wfm/wfm_dsp.c``) three ways:

1. **Impulse response** for roll-off ``beta in {0.0, 0.35, 1.0}`` — the unit-
   energy taps (``sum(h^2) == 1``), ``2*span*sps + 1`` of them, symmetric about
   a peak centre tap.
2. **Magnitude frequency response** — the band-limiting; the −3 dB edge sits
   near ``(1+beta)/(2T)`` and excess bandwidth grows with ``beta``.
3. **Matched-filter cascade (TX⊛RX)** — convolving the RRC with itself gives a
   *raised cosine*, whose defining property is **zero inter-symbol
   interference**: the cascade is zero at every non-zero integer multiple of
   ``sps``. Those zero crossings are marked; that is what lets a receiver
   sample one symbol without smear from its neighbours.

Mirrors the analytic checks in ``TestRRC`` of
``src/doppler/wfm/tests/test_dsp_correctness.py``.

Run:
    python examples/python/wfm_rrc_response.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.wfm import rrc_taps

SPS = 8  # samples per symbol
SPAN = 8  # one-sided support in symbols
BETAS = (0.0, 0.35, 1.0)


def main() -> None:
    fig, (ax_t, ax_f, ax_c) = plt.subplots(3, 1, figsize=(8.0, 9.5))

    for beta in BETAS:
        h = np.asarray(rrc_taps(beta, SPS, SPAN))
        n = len(h)
        assert n == 2 * SPAN * SPS + 1
        # Unit energy is the contract that makes TX⊛RX a unit-gain Nyquist
        # pulse; show it holds.
        energy = float(np.sum(h**2))

        # 1) impulse response, x-axis in symbol periods T.
        t = (np.arange(n) - (n - 1) / 2) / SPS
        ax_t.plot(t, h, label=f"β={beta:g} (Σh²={energy:.3f})")

        # 2) magnitude response over [0, fs/2] in cycles/symbol (×sps -> the
        # symbol rate is 1.0 there).
        nfft = 4096
        H = np.fft.rfft(h, nfft)
        f = np.fft.rfftfreq(nfft, d=1.0 / SPS)  # cycles per symbol
        mag_db = 20.0 * np.log10(np.abs(H) / np.max(np.abs(H)) + 1e-12)
        ax_f.plot(f, mag_db, label=f"β={beta:g}")

        # 3) matched-filter cascade: RRC ⊛ RRC == raised cosine.
        rc = np.convolve(h, h)
        centre = len(rc) // 2
        lag = np.arange(len(rc)) - centre
        ax_c.plot(lag / SPS, rc / rc[centre], label=f"β={beta:g}")

    # Mark the Nyquist sampling instants (non-zero integer symbol lags) where
    # the raised-cosine cascade must be ~0 -> no inter-symbol interference.
    isi_lags = [k for k in range(-SPAN, SPAN + 1) if k != 0]
    ax_c.scatter(
        isi_lags,
        np.zeros(len(isi_lags)),
        c="k",
        s=18,
        zorder=5,
        label="no-ISI zeros",
    )

    ax_t.set_title(f"RRC impulse response (sps={SPS}, span={SPAN})")
    ax_t.set_xlabel("time (symbol periods T)")
    ax_t.set_ylabel("amplitude")
    ax_t.grid(True, alpha=0.3)
    ax_t.legend(fontsize=8)

    ax_f.set_title("RRC magnitude response")
    ax_f.set_xlabel("frequency (cycles per symbol)")
    ax_f.set_ylabel("magnitude (dB)")
    ax_f.set_ylim(-80, 5)
    ax_f.set_xlim(0, SPS / 2)
    ax_f.grid(True, alpha=0.3)
    ax_f.legend(fontsize=8)

    ax_c.set_title(
        "Matched-filter cascade TX⊛RX (raised cosine) — Nyquist no-ISI"
    )
    ax_c.set_xlabel("lag (symbols)")
    ax_c.set_ylabel("normalised amplitude")
    ax_c.set_xlim(-SPAN, SPAN)
    ax_c.grid(True, alpha=0.3)
    ax_c.legend(fontsize=8)

    fig.tight_layout()
    fig.savefig("wfm_rrc_response.png", dpi=110)

    # Numerically confirm the no-ISI property for the β=0.35 cascade.
    h = np.asarray(rrc_taps(0.35, SPS, SPAN))
    rc = np.convolve(h, h)
    centre = len(rc) // 2
    worst = max(
        abs(rc[centre + k * SPS] / rc[centre])
        for k in isi_lags
        if 0 <= centre + k * SPS < len(rc)
    )
    print(f"β=0.35 cascade worst residual ISI at symbol instants: {worst:.2e}")
    print("wrote wfm_rrc_response.png")


if __name__ == "__main__":
    main()
