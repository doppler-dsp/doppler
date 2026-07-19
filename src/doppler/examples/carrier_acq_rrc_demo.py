"""carrier_acq_rrc_demo.py -- doppler.acquire.CarrierAcquisition's
psd_template override, shown against a root-raised-cosine (RRC)
pulse-shaped BPSK stream instead of the default rectangular-NRZ pulse
its built-in template assumes.

CarrierAcquisition is a PSDMF (power-spectral-density matched-filter)
residual-carrier estimator: it non-coherently averages the incoming
stream's power spectrum (:class:`~doppler.spectral.PSD`), then
circularly correlates that average against a KNOWN power spectrum
shape to find the residual carrier offset. The default known shape
(``psd_template`` left empty) is the average PSD of a random
rectangular-pulse (plain NRZ) BPSK stream -- a sinc^2. An RRC-shaped
stream's average PSD is a DIFFERENT shape entirely (the squared
magnitude of the RRC filter's own frequency response, a raised-cosine
roll-off with no sidelobes past ``(1+beta)/(2*sps)`` of the symbol
rate) -- matching this session's finding that the template is a
property of the *pulse shape*, not a universal constant (see
``~/legacy-commz``'s own ``FrequencyAcquisition.power_spectrum``
override, which this object's own ``psd_template`` mirrors).

Two conditions, same signal, same true 137 Hz residual:

- **10 dB Es/N0**: both templates land within a few Hz -- at generous
  margin, template shape barely matters.
- **0 dB Es/N0**: both templates still confidently detect (the CFAR
  gate fires for both -- a corrected threshold, see
  FINISHING_PLAN.md's CarrierAcquisition section, replaced the earlier
  overly-conservative one that used to make the wrong template miss
  outright at moderate SNR), but the DEFAULT (rectangular-pulse)
  template's own estimate is roughly 2x worse than the matched RRC
  template's -- the wrong shape systematically biases the estimate,
  and that bias grows as SNR degrades.

Run:  python -m doppler.examples.carrier_acq_rrc_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.wfm import rrc_taps

SYM_RATE_HZ = 1000.0
SPS = 8  # samples/symbol
SAMPLE_RATE_HZ = SYM_RATE_HZ * SPS
BETA = 0.35  # RRC roll-off
SPAN = 6  # RRC one-sided support, symbols
TRUE_RESIDUAL_HZ = 137.0
N_SYM = 20000
SEED = 1


def make_signal(esn0_db: float, seed: int = SEED):
    """A random BPSK stream, RRC pulse-shaped, carrying a known residual
    carrier and AWGN at the given per-symbol Es/N0. Returns the complex
    baseband capture."""
    rng = np.random.default_rng(seed)
    bits = np.where(rng.integers(0, 2, N_SYM), 1.0, -1.0)
    upsampled = np.zeros(N_SYM * SPS)
    upsampled[::SPS] = bits
    taps = rrc_taps(BETA, SPS, SPAN)
    shaped = np.convolve(upsampled, taps, mode="same")

    sig_power = np.mean(shaped**2)
    noise_power = sig_power / (10.0 ** (esn0_db / 10.0))
    noise = np.sqrt(noise_power / 2.0) * (
        rng.standard_normal(len(shaped)) + 1j * rng.standard_normal(len(shaped))
    )
    t = np.arange(len(shaped))
    tone = np.exp(2j * np.pi * TRUE_RESIDUAL_HZ * t / SAMPLE_RATE_HZ)
    return (shaped * tone + noise).astype(np.complex64)


# --8<-- [end:signal]

from doppler.acquire import CarrierAcquisition  # noqa: E402
from doppler.spectral import PSD  # noqa: E402

NO_TEMPLATE = np.array([], dtype=np.float32)
# design_snr picks a multi-look non-sequential dwell (det_n_noncoh) instead
# of the object's own ergonomic default (design_snr=2.0, dwell_target=1) --
# a single look's own periodogram carries real "self-noise" from the random
# data pattern's own realization, so this compares CONVERGED estimates, not
# single-look noise (see FINISHING_PLAN.md's own CarrierAcquisition section
# for the broader open question about calibrating this properly).
CA_KWARGS = {"design_snr": 0.08, "sequential": False}


# --8<-- [start:templates]
def rrc_template_for(nfft: int) -> np.ndarray:
    """The known average PSD shape of an RRC-shaped random BPSK stream:
    the squared magnitude of the RRC filter's own frequency response,
    DC-centred to match CarrierAcquisition's own bin convention -- a
    linear filter applied to a white bipolar sequence has average PSD
    proportional to |H(f)|^2, the direct RRC analogue of the default
    template's rectangular-pulse sinc^2."""
    taps = rrc_taps(BETA, SPS, SPAN)
    padded = np.zeros(nfft)
    padded[: len(taps)] = taps
    h = np.fft.fftshift(np.fft.fft(padded))
    template = (np.abs(h) ** 2).astype(np.float32)
    return template / template.max()


# --8<-- [end:templates]


# --8<-- [start:compare]
def estimate(x: np.ndarray, template: np.ndarray) -> CarrierAcquisition:
    ca = CarrierAcquisition(template, SAMPLE_RATE_HZ, SYM_RATE_HZ, **CA_KWARGS)
    ca.steps(x)
    return ca


def run_condition(esn0_db: float):
    """Estimate the residual with both the default (wrong-shape) and the
    RRC-matched template, at one Es/N0. Returns a dict of results."""
    x = make_signal(esn0_db)
    probe = CarrierAcquisition(NO_TEMPLATE, SAMPLE_RATE_HZ, SYM_RATE_HZ)
    template = rrc_template_for(probe.nfft)

    default = estimate(x, NO_TEMPLATE)
    rrc = estimate(x, template)
    return {"x": x, "default": default, "rrc": rrc}


# --8<-- [end:compare]


def main(out_path: str = "carrier_acq_rrc_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    hi = run_condition(10.0)
    lo = run_condition(0.0)

    for label, r in (("10 dB", hi), ("0 dB", lo)):
        d, rr = r["default"], r["rrc"]
        print(
            f"{label}: default ready={d.ready} "
            f"residual_hz={d.residual_hz if d.ready else float('nan'):+.1f}  "
            f"rrc ready={rr.ready} residual_hz={rr.residual_hz:+.1f}"
        )

    assert hi["default"].ready and hi["rrc"].ready, (
        "expected both templates to detect at 10 dB"
    )
    assert abs(hi["default"].residual_hz - TRUE_RESIDUAL_HZ) < 10.0
    assert abs(hi["rrc"].residual_hz - TRUE_RESIDUAL_HZ) < 10.0

    assert lo["default"].ready and lo["rrc"].ready, (
        "expected both templates to still confidently detect at 0 dB "
        "with the corrected CFAR threshold (carrier_acq_core.c's "
        "_ratio_threshold -- see FINISHING_PLAN.md's CarrierAcquisition "
        "section)"
    )
    default_err = abs(lo["default"].residual_hz - TRUE_RESIDUAL_HZ)
    rrc_err = abs(lo["rrc"].residual_hz - TRUE_RESIDUAL_HZ)
    assert default_err > 1.5 * rrc_err, (
        "expected the WRONG (rectangular-pulse) template's estimate to "
        "be meaningfully worse than the matched RRC template's at "
        "degraded SNR -- if this now fails, the demo's own headline "
        "point (template shape biases accuracy) no longer holds at "
        "these parameters and needs re-tuning"
    )
    assert rrc_err < 10.0

    # --- illustrative measured spectrum (NOT read from CarrierAcquisition's
    # own internals -- a separate PSD instance, for the plot only) ---------
    nfft = hi["default"].nfft if hi["default"].ready else 512
    psd = PSD(n=nfft // 4, fs=SAMPLE_RATE_HZ, window="hann", mode="mean")
    x5 = lo["x"]
    n_frame = psd.n
    for i in range(0, (len(x5) // n_frame) * n_frame, n_frame):
        psd.accumulate(x5[i : i + n_frame].astype(np.complex64))
    freqs = np.fft.fftshift(np.fft.fftfreq(psd.nfft, d=1.0 / SAMPLE_RATE_HZ))
    measured_db = psd.psd_db()

    rrc_tmpl_plot = rrc_template_for(psd.nfft)
    rrc_db = 10.0 * np.log10(rrc_tmpl_plot + 1e-6)

    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4.5))

    a.plot(freqs, measured_db - measured_db.max(), color="#1f77b4", lw=1.0,
           label="measured PSD (0 dB Es/N0)")
    a.plot(freqs, rrc_db - rrc_db.max(), color="#2ca02c", lw=1.4, ls="--",
           label="RRC template (matched)")
    a.axvline(TRUE_RESIDUAL_HZ, color="k", lw=0.8, ls=":", label="true residual")
    a.set_xlim(-SAMPLE_RATE_HZ / 2, SAMPLE_RATE_HZ / 2)
    a.set_ylim(-40, 5)
    a.set_xlabel("frequency (Hz)")
    a.set_ylabel("dB (peak-normalised)")
    a.set_title("Measured spectrum vs. the matched RRC template", fontsize=9)
    a.legend(fontsize=7)
    a.grid(alpha=0.25)

    conditions = ["10 dB\ndefault", "10 dB\nRRC", "0 dB\ndefault", "0 dB\nRRC"]
    errs = [
        hi["default"].residual_hz - TRUE_RESIDUAL_HZ,
        hi["rrc"].residual_hz - TRUE_RESIDUAL_HZ,
        np.nan if not lo["default"].ready else lo["default"].residual_hz - TRUE_RESIDUAL_HZ,
        lo["rrc"].residual_hz - TRUE_RESIDUAL_HZ,
    ]
    colors = ["#7f9ec9", "#2ca02c", "#d62728", "#2ca02c"]
    bars = b.bar(conditions, [0 if np.isnan(e) else e for e in errs], color=colors)
    finite = [e for e in errs if not np.isnan(e)]
    y_lo, y_hi = min(finite + [0.0]) * 1.4 - 0.3, max(finite + [0.0]) * 1.4 + 0.3
    b.set_ylim(y_lo, y_hi)
    label_y = y_lo + 0.15 * (y_hi - y_lo)
    for bar, e in zip(bars, errs):
        if np.isnan(e):
            b.text(bar.get_x() + bar.get_width() / 2, label_y, "NOT\nDETECTED",
                   ha="center", va="center", fontsize=7, color="white",
                   bbox=dict(boxstyle="round", facecolor="#d62728", edgecolor="none"))
    b.axhline(0, color="k", lw=0.6)
    b.set_ylabel("residual_hz - true (Hz)")
    b.set_title("Estimate error: wrong vs. matched template", fontsize=9)
    b.grid(alpha=0.25, axis="y")

    fig.suptitle(
        f"CarrierAcquisition psd_template override -- RRC-shaped BPSK, "
        f"{SYM_RATE_HZ:.0f} sym/s, true residual {TRUE_RESIDUAL_HZ:.0f} Hz",
        fontsize=10,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "carrier_acq_rrc_demo.png")
