"""A streaming spectrogram of a tone that changes frequency."""

import numpy as np

from doppler.spectral import Spectrogram

NFFT = 256
FS = 1.0e6
N_SEG = 8 * NFFT  # each tone is held for eight frames (rows)


def main() -> None:
    # The tone sits on bin 32 for the first half, then on bin 96. A bin is
    # fs / nfft wide, so this is a jump of 64 bins, 250 kHz at FS = 1 MHz.
    n = np.arange(2 * N_SEG)
    bins = np.where(n < N_SEG, 32, 96)
    x = np.exp(2j * np.pi * bins * n / NFFT).astype(np.complex64)

    sp = Spectrogram(nfft=NFFT, hop=NFFT, window="hann", mode="power")

    rows = []
    rng = np.random.default_rng(0)
    pos = 0
    while pos < x.size:
        # Chunks of arbitrary size, not multiples of NFFT.
        size = int(rng.integers(1, 3 * NFFT))
        block = sp.push(x[pos : pos + size])
        if block is not None:  # None: this push completed no row
            rows.append(block)
        pos += size
    tail = np.empty(NFFT, np.float32)
    if sp.flush(tail):  # a trailing partial frame, zero-padded, if one is owed
        rows.append(tail[None, :])
    spectrogram = np.concatenate(rows)

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


if __name__ == "__main__":
    main()
