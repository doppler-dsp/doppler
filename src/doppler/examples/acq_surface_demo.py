#!/usr/bin/env python3
"""acq_surface_demo.py -- watch the searcher: the 2-D test statistic and the
dwell-by-dwell probes of a continuous :class:`~doppler.dsss.Acquisition`.

Design §2.4: the searcher is the one stage a hit alone cannot vouch for,
so the engine carries its own instruments, attach-on-demand. This page
turns both of them on over one emitter and plots what they show.

- **The surface.** ``keep_surface = 1`` makes the engine keep every
  decided dwell's surface in the gate's own units -- each cell divided by
  the CFAR reference the gate used, so a cell *is* its own test statistic
  and the gate is a flat plane. ``surface(out)`` copies it out;
  ``surface_doppler_hz()`` and ``surface_chip_phase()`` are its axes, from
  the same fold and chip-phase mapping the ``DetectionEvent`` carries, so
  the plotted peak sits where the hand-off says.
- **The probes.** ``set_telemetry(tlm, "acq")`` registers ten records per
  decided dwell -- the statistic and its gate, the reference, the peak and
  where it is, the pick count and the held twins, the peak's concentration,
  and whether the gate fired. Read back with a ``Telemetry`` context, they
  are the searcher's own log: where a hit fired, and how close the misses
  came.

The emitter is the shipped continuous-DSSS synth (a Gold code with
asynchronous PRBS data) at a known Doppler and code phase in real noise,
8 dB above the C/N0 the search is sized for, tiled over +-20 kHz. The
asserts are physics: the peak of the plotted surface is the reported
cell, its value is the reported statistic, the axis under it is the
injected Doppler to within a tile and the injected phase to within a
chip, and the gate fired on every dwell of a healthy signal.

Run:  python -m doppler.examples.acq_surface_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:setup]
import numpy as np

from doppler.dsss import Acquisition
from doppler.telemetry import Telemetry
from doppler.wfm import Composer, Gold, Segment

SF = 1023  # the CCSDS command-link Gold code period
CHIP_RATE = 3.0e6  # Hz
SYM_RATE = 2700.0  # Hz, asynchronous to the chips
SPC = 2
FS = CHIP_RATE * SPC
TE = SF * SPC  # samples per code epoch
DOPPLER_HZ = 7250.0  # inside the +-20 kHz search, between two tiles
PHASE_CHIPS = 300.0  # the emitter's code phase, in chips
CN0_DBHZ = 60.0  # the emitter
DESIGN_CN0_DBHZ = 52.0  # what the search is sized for: 8 dB in hand
UNCERTAINTY_HZ = 20.0e3
N_DWELLS = 24
SEED = 3
CODE = Gold().generate(SF)


def make_signal(n_samples: int) -> np.ndarray:
    """One emitter: the continuous-DSSS synth at DOPPLER_HZ, in AWGN at
    CN0_DBHZ, arriving PHASE_CHIPS chips into its code -- the phase is
    the discarded prefix, exactly how the C harnesses place an emitter."""
    payload = np.random.default_rng(SEED).integers(0, 2, 4096, np.uint8)
    prefix = round(PHASE_CHIPS * SPC)
    seg = Segment(
        type="dsss",
        fs=FS,
        sps=SPC,
        freq=DOPPLER_HZ,
        snr=CN0_DBHZ - 10.0 * np.log10(FS),
        snr_mode="fs",
        seed=SEED,
        data_code=bytes(CODE.tolist()),
        symbol_rate=SYM_RATE,
        payload=bytes(payload.tolist()),
        num_samples=n_samples + prefix,
    )
    return Composer([seg]).compose()[prefix:]


# --8<-- [end:setup]


# --8<-- [start:watch]
def watch() -> dict:
    """Run the searcher over the emitter with both instruments attached.

    Returns the last dwell's surface with its axes, the hits, and the
    probe series -- everything the figure and the asserts need."""
    acq = Acquisition(
        CODE,
        spc=SPC,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        cn0_dbhz=DESIGN_CN0_DBHZ,
        doppler_uncertainty=UNCERTAINTY_HZ,
        pfa=1e-3,
        pd=0.9,
    )
    tlm = Telemetry(1 << 14)
    acq.set_telemetry(tlm, "acq")  # ten probes per decided dwell
    acq.keep_surface = 1  # keep each decided dwell's surface

    per_dwell = acq.n_noncoh * TE
    x = make_signal(N_DWELLS * per_dwell)
    hits = acq.push(x)

    surface = np.empty(acq.surface_rows * acq.code_bins, np.float32)
    assert acq.surface(surface) == surface.size
    surface = surface.reshape(acq.surface_rows, acq.code_bins)
    doppler_hz = np.empty(acq.surface_rows)
    chip_phase = np.empty(acq.code_bins)
    acq.surface_doppler_hz(doppler_hz)
    acq.surface_chip_phase(chip_phase)

    # The probes, demultiplexed by name: one value per decided dwell.
    recs = tlm.read()
    ids = tlm.probe_names
    series = {
        name: recs["value"][recs["probe"] == pid] for name, pid in ids.items()
    }
    gate = float(acq.eta_nc if acq.n_noncoh > 1 else acq.threshold)
    return {
        "acq": acq,
        "hits": hits,
        "surface": surface,
        "doppler_hz": doppler_hz,
        "chip_phase": chip_phase,
        "series": series,
        "gate": gate,
    }


# --8<-- [end:watch]


def main(out_path: str = "acq_surface_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    w = watch()
    acq, hits, surf = w["acq"], w["hits"], w["surface"]
    fhz, chips, series, gate = (
        w["doppler_hz"],
        w["chip_phase"],
        w["series"],
        w["gate"],
    )

    # ── the asserts: the instruments agree with the hit, and with physics ──
    n_dwells = series["acq.hit"].size
    assert n_dwells == N_DWELLS, (n_dwells, N_DWELLS)
    assert np.all(series["acq.hit"] == 1.0), (
        "a healthy emitter fires every dwell"
    )
    assert np.all(series["acq.stat"] > gate)
    # a hit is (doppler_bin, code_phase, peak_mag, noise_est, test_stat,
    # cn0_dbhz_est, samples_consumed)
    last = hits[-1]
    row, col = np.unravel_index(np.argmax(surf), surf.shape)
    assert (row, col) == (last[0], last[1]), (row, col)
    # to a float rounding: the SIMD build's fast-math may normalise the
    # surface with a reciprocal where the statistic took a divide
    assert np.isclose(surf[row, col], last[4], rtol=4e-7, atol=0)
    assert np.isclose(surf[row, col], series["acq.stat"][-1], rtol=4e-7)
    tile_hz = CHIP_RATE / SF
    assert abs(fhz[row] - DOPPLER_HZ) <= tile_hz / 2 + 1e-6, fhz[row]
    assert abs(chips[col] - PHASE_CHIPS) <= 1.0, chips[col]
    assert acq.peak_conc > 0.5, acq.peak_conc

    # ── the figure: the surface, and the dwell log under it ──
    # both axes come out in the engine's order (the FFT fold, the phase
    # mapping); a plot wants them monotonic
    ro, co = np.argsort(fhz), np.argsort(chips)
    fig, (ax_s, ax_t) = plt.subplots(
        2, 1, figsize=(9.5, 8.0), gridspec_kw={"height_ratios": [3, 1.4]}
    )
    im = ax_s.pcolormesh(
        chips[co],
        fhz[ro] / 1e3,
        surf[np.ix_(ro, co)],
        shading="nearest",
        cmap="magma",
    )
    ax_s.plot(chips[col], fhz[row] / 1e3, "c+", ms=14, mew=2, label="hit")
    ax_s.axhline(DOPPLER_HZ / 1e3, color="w", lw=0.6, ls=":", label="injected")
    ax_s.axvline(PHASE_CHIPS, color="w", lw=0.6, ls=":")
    ax_s.set_xlabel("code phase (chips)")
    ax_s.set_ylabel("Doppler (kHz)")
    ax_s.set_title(
        f"the last dwell's surface, in the gate's units (gate = {gate:.2f}); "
        f"peak {surf[row, col]:.1f} at {fhz[row] / 1e3:+.2f} kHz, "
        f"{chips[col]:.1f} chips"
    )
    fig.colorbar(im, ax=ax_s, label="test statistic")
    ax_s.legend(loc="upper right")

    d = np.arange(1, n_dwells + 1)
    ax_t.plot(d, series["acq.stat"], "o-", ms=3, label="acq.stat")
    ax_t.axhline(gate, color="r", lw=1, label="acq.gate")
    ax_t.plot(d, 10 * series["acq.conc"], "s-", ms=3, label="10 x acq.conc")
    ax_t.set_xlabel("decided dwell")
    ax_t.set_ylabel("statistic")
    ax_t.set_title("the probes, one record per decided dwell")
    ax_t.legend(loc="lower right", ncol=3)
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(
        f"wrote {out_path}: {n_dwells} dwells, peak {surf[row, col]:.1f} vs "
        f"gate {gate:.2f}, conc {acq.peak_conc:.2f}"
    )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "acq_surface_demo.png")
