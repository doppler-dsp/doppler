"""wfm_receiver_ber.py — BER vs Eb/No for wfmgen BPSK / QPSK.

End-to-end validation of the ``wfmgen`` SNR model: generate BPSK and QPSK at a
set of Eb/No points with ``snr_mode="ebno"``, decide the symbols, measure the
bit-error rate, and overlay the closed-form coherent curve
``BER = Q(sqrt(2 Eb/No))``. Agreement across the sweep is the proof that the
``ebno`` SNR anchoring (``native/src/wfm/wfm_resolve.c``) places exactly the
right noise power.

Why this works at one sample per symbol: with ``snr_mode="ebno"`` and ``sps=1``
the synth sets the per-component noise variance so a Gray-coded BPSK/QPSK
slicer sees ``Eb/No`` directly, and both schemes share the same theoretical
BER. The transmitted symbols are recovered by re-generating the *same seed* at
a clean SNR (the synth's data stream is independent of its AWGN stream), so
only the noise differs between the reference and the received copy — see the
``test_determinism`` checks in
``src/doppler/wfm/tests/test_dsp_correctness.py``.

Run:
    python examples/python/wfm_receiver_ber.py
"""

from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.special import erfc

from doppler.wfm import Synth

EBNO_DB = np.array([0.0, 2.0, 4.0, 6.0, 8.0, 10.0])
N_SYM = 40_000  # symbols per point (sps=1 -> samples == symbols)
SEED = 12345


def _q(x: np.ndarray) -> np.ndarray:
    """Gaussian tail probability ``Q(x) = 0.5 * erfc(x / sqrt(2))``."""
    return 0.5 * erfc(x / np.sqrt(2.0))


def _measure_ber(kind: str, ebno_db: float) -> float:
    """Generate ``kind`` at ``ebno_db``, slice, and return the measured BER.

    The clean (snr=100 dB) re-generation with the same seed is the transmitted
    constellation; the noisy copy is the receiver input. Bits come straight off
    the sign of each quadrature (Gray-coded), so the BER needs no explicit bit
    map.
    """
    common = {
        "type": kind,
        "fs": 1.0,
        "snr_mode": "ebno",
        "sps": 1,
        "seed": SEED,
    }
    ref = np.asarray(Synth(snr=100.0, **common).steps(N_SYM))
    rx = np.asarray(Synth(snr=ebno_db, **common).steps(N_SYM))

    if kind == "bpsk":
        tx_bits = ref.real < 0.0
        rx_bits = rx.real < 0.0
        return float(np.mean(tx_bits != rx_bits))
    # QPSK: one Gray bit per quadrature.
    tx_bits = np.concatenate([ref.real < 0.0, ref.imag < 0.0])
    rx_bits = np.concatenate([rx.real < 0.0, rx.imag < 0.0])
    return float(np.mean(tx_bits != rx_bits))


def main() -> None:
    theory = _q(np.sqrt(2.0 * 10.0 ** (EBNO_DB / 10.0)))

    fig, ax = plt.subplots(figsize=(7.5, 5.0))
    ax.semilogy(EBNO_DB, theory, "k--", label=r"theory  $Q(\sqrt{2E_b/N_0})$")

    print(f"{'Eb/No':>6} {'scheme':>6} {'measured':>12} {'theory':>12}")
    for kind, marker in (("bpsk", "o"), ("qpsk", "s")):
        ber = np.array([_measure_ber(kind, e) for e in EBNO_DB])
        ax.semilogy(EBNO_DB, ber, marker, label=kind.upper())
        for e, m, t in zip(EBNO_DB, ber, theory):
            print(f"{e:6.1f} {kind:>6} {m:12.2e} {t:12.2e}")
        # The two highest-SNR points should track theory within ~30%
        # (Monte-Carlo spread at N_SYM symbols); a gross model error would be
        # orders of magnitude off.
        hi = ber[-2:] > 0
        if np.any(hi):
            ratio = ber[-2:][hi] / theory[-2:][hi]
            assert np.all((ratio > 0.4) & (ratio < 2.5)), (
                f"{kind} BER departs from theory: {ratio}"
            )

    ax.set_xlabel("Eb/No (dB)")
    ax.set_ylabel("bit-error rate")
    ax.set_title("wfmgen BPSK / QPSK — measured BER vs theory")
    ax.set_ylim(1e-5, 1.0)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig("wfm_receiver_ber.png", dpi=110)
    print("wrote wfm_receiver_ber.png")


if __name__ == "__main__":
    main()
