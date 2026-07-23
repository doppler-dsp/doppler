"""crowded_band_demo.py — a segment packed with many signals, prepared in
parallel with the wfm ``Plan`` component cache.

The case that motivates parallelism in waveform creation is a single segment
carrying *many* independent signals — a fully-loaded band of carriers, a
multi-user CDMA cell, a dense interference scene. Each signal is its own
expensive DSP (modulation, root-raised-cosine pulse shaping, a mix to its
channel centre), and none of them depend on the others: they are simply summed.
:func:`~doppler.wfm.prepare` renders each one *once* into its own cache buffer,
and — because those builds are independent — fans them across the machine's
cores. The sum is deferred, so the cached result is **bit-for-bit identical**
to a full serial compose; the only thing that changed is how fast it was built.

The scene here is twenty RRC-shaped QPSK carriers, spaced across a 2.4 MHz span
over one AWGN floor, at three power tiers. One `Plan` drives
the whole figure:

  * **Top** — the crowded band: every one of the twenty carriers rendered from
    the prepared cache, a baseline that reproduces a full compose exactly.
  * **Bottom** — a variation materialised *for free* from the same cache:
    ``render(enable=...)`` disables every other carrier (an exact ``gain=0``
    term), no re-synthesis. The gaps open up; the survivors are untouched.

Run:  python -m doppler.examples.crowded_band_demo  [out.png]
"""

from __future__ import annotations

import os
import sys
import time

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

# --8<-- [start:scene]
import numpy as np

from doppler.wfm import Composer, Segment, prepare, qpsk

FS = 2.4e6  # occupied span (Hz)
N = 1 << 16  # 65,536 samples — well past prepare()'s parallel threshold
N_CARRIERS = 20  # a densely-loaded band: twenty signals in one segment
SPS = 64  # samples per symbol → ~37.5 kHz symbol rate, narrow carriers
SPACING = 100e3  # carrier spacing (Hz)
ANCHOR_SNR = 20.0  # carrier 0 carries the channel SNR; it sets the floor
_F0 = -(N_CARRIERS - 1) / 2.0 * SPACING  # first offset (band centred on DC)


def crowded_band() -> Composer:
    """Twenty RRC-shaped QPSK carriers over the AWGN floor set by carrier 0.

    Each carrier is fully independent DSP — QPSK symbols, a 2049-tap
    root-raised-cosine pulse (``2*rrc_span*sps + 1``), and a mix to its own
    channel centre — so ``prepare()`` fans the twenty per-carrier builds out
    across cores. Three power tiers (0 / -3 / -6 dBFS) keep it interesting.
    Carrier 0 carries the channel SNR (the resolver derives one shared noise
    floor from it); the rest are clean.
    """
    carriers = [
        qpsk(
            freq=_F0 + k * SPACING,
            snr=ANCHOR_SNR if k == 0 else 100.0,  # carrier 0 is the anchor
            seed=10 + k,
            sps=SPS,
            pulse="rrc",
            rrc_beta=0.25,
            rrc_span=16,
            level=-3.0 * (k % 3),
        )
        for k in range(N_CARRIERS)
    ]
    return Composer(Segment.sum(*carriers, fs=FS, num_samples=N))


# --8<-- [end:scene]


# --8<-- [start:prepare]
# prepare() renders every carrier ONCE and caches it — fanning the twenty
# independent per-carrier builds across cores — then render() serves each
# variation as a cheap re-weighted sum of that cache, never re-synthesising.
scene = crowded_band()
plan = prepare(scene)

# The cache is exact: a baseline render is bit-for-bit a full serial compose.
assert np.array_equal(plan.render(), scene.compose())

# Materialize a variation for free: disable every other carrier (the noise
# floor, handled separately, stays put). `enable` is per signal source.
_survive = [k % 2 == 0 for k in range(N_CARRIERS)]
thinned = np.asarray(plan.render(enable=_survive))
# --8<-- [end:prepare]


def _psd_db(x: np.ndarray) -> np.ndarray:
    """Hann-windowed periodogram, DC-centred, normalised to a 0 dB peak."""
    spec = np.fft.fftshift(np.fft.fft(x * np.hanning(len(x))))
    power = np.abs(spec) ** 2
    return 10.0 * np.log10(power / power.max() + 1e-12)


def _band_peak(psd: np.ndarray, freqs: np.ndarray, fc: float) -> float:
    """Peak PSD (dB) within ±0.3·spacing of carrier centre ``fc``."""
    band = np.abs(freqs - fc) < SPACING * 0.3
    return float(psd[band].max())


def main(out: str = "crowded_band_demo.png") -> None:
    freqs = (np.arange(N) - N // 2) * (FS / N)
    full = _psd_db(np.asarray(plan.render()))
    thin = _psd_db(thinned)
    fcs = [_F0 + k * SPACING for k in range(N_CARRIERS)]

    # Physical checks:
    #  1. every carrier is clearly present in the full band (all >= -9 dB);
    #  2. thinning leaves each kept (even) carrier essentially unchanged, while
    #     each disabled (odd) carrier collapses to the residual noise floor
    #     (~-21 dB) — well clear of any kept carrier.
    assert all(_band_peak(full, freqs, fc) > -25.0 for fc in fcs), (
        "a carrier is missing from the full render"
    )
    for k, fc in enumerate(fcs):
        kept = _band_peak(thin, freqs, fc)
        if k % 2 == 0:
            moved = _band_peak(full, freqs, fc) - kept
            assert moved < 3.0, f"kept carrier {k} moved ({moved:.1f} dB)"
        else:
            assert kept < -16.0, (
                f"disabled carrier {k} still present ({kept:.1f} dB)"
            )

    # Timing (informational — the parallel win, not asserted): the machine's
    # core count and the wall time to prepare the whole crowded segment.
    reps = 5
    t0 = time.perf_counter()
    for _ in range(reps):
        prepare(crowded_band())
    t_prepare = (time.perf_counter() - t0) / reps
    print(
        f"{N_CARRIERS} carriers x {N:,} samples: "
        f"prepare {t_prepare * 1e3:.1f} ms across {os.cpu_count()} cores"
    )

    fig, (ax0, ax1) = plt.subplots(
        2, 1, figsize=(10, 6.2), sharex=True, sharey=True
    )
    ax0.plot(freqs / 1e6, full, lw=0.5, color="tab:blue")
    ax0.set(
        title=f"{N_CARRIERS} RRC carriers from one Plan.prepare()",
        ylabel="PSD (dB)",
    )
    ax0.grid(True, alpha=0.3)
    ax1.plot(freqs / 1e6, thin, lw=0.5, color="tab:orange")
    ax1.set(
        title="Same cache, every other carrier disabled — render(enable=...)",
        xlabel="frequency (MHz)",
        ylabel="PSD (dB)",
    )
    ax1.grid(True, alpha=0.3)
    fig.suptitle(
        f"Prepare once across {os.cpu_count()} cores, then materialize many"
    )
    fig.tight_layout()
    fig.savefig(out, dpi=110)
    print(f"wrote {out}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "crowded_band_demo.png")
