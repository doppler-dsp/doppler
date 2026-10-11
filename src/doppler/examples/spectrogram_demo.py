"""A streaming spectrogram of a chirp and a tone, in power and dB rows."""

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from doppler.spectral import Spectrogram

NFFT = 256
HOP = 64
FS = 1.0e6
N_SEG = 8 * NFFT  # each tone is held for eight frames (rows)
# 96 samples past whole frames: the stream then owes a partial frame.
CAPTURE = 16 * NFFT * 4 + 96


# --8<-- [start:stream]
def stream_rows(
    x: np.ndarray, mode: str, hop: int, chunked: bool = True
) -> np.ndarray:
    """Return every row a stream produces, pushed in chunks or all at once."""
    sp = Spectrogram(nfft=NFFT, hop=hop, window="hann", mode=mode)
    rows = []
    rng = np.random.default_rng(0)
    pos = 0
    while pos < x.size:
        size = int(rng.integers(1, 3 * NFFT)) if chunked else x.size
        block = sp.push(x[pos : pos + size])
        if block is not None:  # None: this push completed no row
            rows.append(block)
        pos += size
    tail = np.empty(NFFT, np.float32)
    if sp.flush(tail):  # a trailing partial frame, zero-padded, if one is owed
        rows.append(tail[None, :])
    return np.concatenate(rows)


# --8<-- [end:stream]


# --8<-- [start:figure]
def make_figure(out_path: str = "spectrogram_demo.png") -> None:
    """Draw the chirp and tone as power and dB rows, after checking them."""
    n = np.arange(CAPTURE)
    t = n / FS
    total = CAPTURE / FS
    f0, f1 = -0.4 * FS, 0.4 * FS
    k = (f1 - f0) / total
    chirp = np.exp(2j * np.pi * (f0 * t + 0.5 * k * t * t))
    # On a bin centre, so the reading is the bin's own -6.02 dB.
    f_tone = 51 * FS / NFFT
    tone = 0.5 * np.exp(2j * np.pi * f_tone * t)
    x = (chirp + tone).astype(np.complex64)

    power = stream_rows(x, "power", HOP)
    db = stream_rows(x, "db", HOP)
    assert power.shape == db.shape
    # 254 whole frames, then the owed partial one, flushed at the end.
    assert power.shape[0] == 255
    # Chunking must not change the rows: compare with one push.
    assert np.array_equal(power, stream_rows(x, "power", HOP, chunked=False))

    bin_hz = FS / NFFT
    freqs = np.fft.fftshift(np.fft.fftfreq(NFFT, 1 / FS))
    # The bins span -fs/2 to +fs/2 exactly, so the axis edges are these.
    assert np.isclose(freqs[0], -FS / 2)
    assert np.isclose(freqs[-1] + bin_hz, FS / 2)

    rows = np.arange(power.shape[0])
    centre = (rows * HOP + NFFT / 2) / FS
    expect_hz = f0 + k * centre
    far = np.abs(expect_hz - f_tone) > 4 * bin_hz
    # The chirp's peak tracks the sweep within one bin, the flushed row too.
    peak_hz = (power.argmax(axis=1) - NFFT // 2) * bin_hz
    assert np.all(np.abs(peak_hz[far] - expect_hz[far]) <= bin_hz), np.abs(
        peak_hz[far] - expect_hz[far]
    ).max()
    tone_bin = NFFT // 2 + 51
    # The tone reads a quarter of full scale, and -6.02 dB, on every row clear
    # of the chirp.
    assert far.sum() > 200
    assert np.all(np.abs(power[far, tone_bin] - 0.25) < 0.02)
    assert np.all(np.abs(db[far, tone_bin] + 6.02) < 0.2)
    # Each dB row is 10 log10 of its power row, above the -90 dB floor.
    live = power > 1e-9
    assert np.allclose(db[live], 10 * np.log10(power[live]), atol=1e-3)

    fig, (ax_p, ax_d) = plt.subplots(1, 2, figsize=(11, 5), sharey=True)
    # MHz across; ms down, with row 0 (time 0) at the top of the panel.
    extent = (freqs[0] / 1e6, (freqs[-1] + bin_hz) / 1e6, total * 1e3, 0.0)
    im_p = ax_p.imshow(
        power,
        origin="upper",
        aspect="auto",
        extent=extent,
        vmin=0.0,
        vmax=1.0,
        cmap="magma",
    )
    im_d = ax_d.imshow(
        db,
        origin="upper",
        aspect="auto",
        extent=extent,
        vmin=-90.0,
        vmax=0.0,
        cmap="magma",
    )
    ax_p.set_title("power rows (linear, the default)")
    ax_d.set_title("dB rows (dBFS, asked for by name)")
    for ax in (ax_p, ax_d):
        ax.set_xlabel("frequency (MHz, DC-centred)")
        ax.axvline(0.0, color="w", lw=0.5, ls=":")
    ax_p.set_ylabel("time (ms)")
    fig.colorbar(im_p, ax=ax_p, label="power (full scale = 1)")
    fig.colorbar(im_d, ax=ax_d, label="level (dBFS)")
    fig.suptitle("A chirp and a tone, one row per 64 samples")
    fig.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)


# --8<-- [end:figure]


def main() -> None:
    # The tone sits on bin 32 for the first half, then on bin 96. A bin is
    # fs / nfft wide, so this is a jump of 64 bins, 250 kHz at FS = 1 MHz.
    n = np.arange(2 * N_SEG)
    bins = np.where(n < N_SEG, 32, 96)
    x = np.exp(2j * np.pi * bins * n / NFFT).astype(np.complex64)

    spectrogram = stream_rows(x, "power", NFFT)

    # Physical checks, each one a claim about the result:
    # 1. Every whole frame produced exactly one row, and none is missing.
    assert spectrogram.shape == (x.size // NFFT, NFFT)
    # 2. The strongest bin of each row is the tone's bin for that row.
    expect = np.where(np.arange(spectrogram.shape[0]) < 8, 32, 96)
    # (Bins are DC-centred, so bin b sits at column NFFT // 2 + b.)
    got = spectrogram.argmax(axis=1) - NFFT // 2
    assert np.array_equal(got, expect), (got, expect)
    # 3. A unit tone reads 1.0 in power mode, to within the window's leakage
    #    into the peak bin's neighbours.
    peak = spectrogram.max(axis=1)
    assert np.all(np.abs(peak - 1.0) < 0.05), peak.min()

    print(
        f"{spectrogram.shape[0]} rows of {NFFT} bins; "
        f"tone moved 32 -> 96 at row 8; peak power {peak.mean():.3f}"
    )
    make_figure()


if __name__ == "__main__":
    main()
