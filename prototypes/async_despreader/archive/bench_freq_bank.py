"""Benchmark: realizing the 34 parallel 3 kHz-spaced frequency bins for
the SPEC.md `D=1` pure-code-phase wideband ACQ search (task #70).

`SPEC.md`'s "## Acquisition" section settled the architecture question
(pure code-phase search at coherent depth `D=1`, SNR margin from
non-coherent accumulation, 34 candidate frequency bins 3 kHz apart
spanning +/-50 kHz, run in parallel) but explicitly left OPEN how to
realize the per-bin down-conversion efficiently. Two candidates, both
reusing doppler.spectral.FFT (the one real FFT primitive -- no
reimplementation) and doppler.source.LO-equivalent tone generation:

(A) **Roll the received epoch's own FFT spectrum.** The required 3 kHz
    hypothesis spacing is EXACTLY this N=2046-sample epoch's own FFT
    bin spacing (`fs/N = chip_rate*spc/(sf*spc) = chip_rate/sf`), so an
    integer-bin circular roll of one forward FFT is mathematically
    identical (not an approximation) to a k-bin frequency de-shift in
    the time domain. Marginal cost per epoch: 1 forward FFT (the
    received signal) + 34 x (roll + multiply + inverse FFT) = 35
    FFT-equivalents. The replica's own FFT is fixed for the whole
    acquisition and precomputed once -- not part of the per-epoch cost.

(B) **A tuned mixer bank.** 34 independent mixers down-convert the SAME
    received epoch to 34 candidate centers; each mixed copy is
    forward-FFT'd and correlated (multiply + inverse FFT) against the
    SAME fixed replica spectrum. The 34 mixing tones are themselves
    fixed and periodic over one epoch (same reasoning as the replica
    FFT in (A) -- the hypothesis spacing is an exact multiple of the
    epoch's own fundamental), so they too are precomputed once, not
    part of the per-epoch marginal cost. Marginal cost per epoch: 34
    forward FFTs + 34 inverse FFTs = 68 FFT-equivalents -- irreducibly
    double (A)'s FFT count, since each hypothesis needs its OWN
    forward FFT of a differently-mixed copy of the input.

Correctness is cross-checked first (both must land on the identical
true frequency-bin/code-phase cell injected into a synthetic epoch)
before the wall-clock race decides anything -- a fast wrong answer is
worthless.
"""
from __future__ import annotations

import time

import numpy as np

from doppler.spectral import FFT

CHIP_RATE = 3.069e6  # SPEC.md nominal chip rate
SF = 1023  # Gold/MLS code length (one epoch)
SPC = 2  # samples/chip
N = SF * SPC  # code_bins = 2046, one epoch, this benchmark's FFT length
FS = CHIP_RATE * SPC  # sample rate
BIN_HZ = CHIP_RATE / SF  # native FFT bin spacing == required hyp. spacing
UNCERTAINTY_HZ = 50_000.0  # +/- 50 kHz, SPEC.md
N_BINS = int(np.ceil(2 * UNCERTAINTY_HZ / BIN_HZ))  # 34
K_RANGE = np.arange(N_BINS) - N_BINS // 2  # candidate bin offsets, ~centered


def _replica(seed=7):
    rng = np.random.default_rng(seed)
    chips = np.where(rng.integers(0, 2, SF).astype(bool), -1.0, 1.0)
    return np.repeat(chips, SPC).astype(np.complex64)


def _rx_epoch(replica, true_k, code_phase, snr_db, seed=3):
    """One code epoch: replica at `code_phase`, shifted `true_k` bins."""
    rng = np.random.default_rng(seed)
    r = np.roll(replica, code_phase)
    n = np.arange(N)
    tone = np.exp(2j * np.pi * true_k * n / N).astype(np.complex64)
    rx = (r * tone).astype(np.complex64)
    p = np.sqrt(np.mean(np.abs(rx) ** 2))
    std = np.sqrt(10 ** (-snr_db / 10)) * p
    noise = rng.normal(0, std / np.sqrt(2), N) + 1j * rng.normal(
        0, std / np.sqrt(2), N
    )
    return (rx + noise).astype(np.complex64)


class RollFftBank:
    """(A) One forward FFT of rx; roll its spectrum per hypothesis."""

    def __init__(self, replica):
        self.fwd = FFT(N, -1, 1)
        self.inv = FFT(N, +1, 1)
        self.replica_conj = np.conj(self.fwd.execute_cf32(replica))

    def search(self, rx):
        rx_fft = self.fwd.execute_cf32(rx)
        best = None
        for k in K_RANGE:
            # de-shift the hypothesis: roll by -k undoes a +k modulation
            shifted = np.roll(rx_fft, -int(k))
            corr = self.inv.execute_cf32(
                (shifted * self.replica_conj).astype(np.complex64)
            )
            mag = np.abs(corr)
            i = int(np.argmax(mag))
            if best is None or mag[i] > best[0]:
                best = (mag[i], int(k), i)
        return best  # (peak_mag, k_bin, code_phase)


class MixerBank:
    """(B) 34 independent tuned mixers, each forward+inverse FFT'd."""

    def __init__(self, replica):
        self.fwd = FFT(N, -1, 1)
        self.inv = FFT(N, +1, 1)
        self.replica_conj = np.conj(self.fwd.execute_cf32(replica))
        n = np.arange(N)
        # Precomputed once (fixed, periodic over one epoch) -- not
        # marginal per-epoch cost, exactly like the replica FFT in (A).
        self.tones = [
            np.exp(-2j * np.pi * int(k) * n / N).astype(np.complex64)
            for k in K_RANGE
        ]

    def search(self, rx):
        best = None
        for k, tone in zip(K_RANGE, self.tones):
            mixed = (rx * tone).astype(np.complex64)
            rx_fft = self.fwd.execute_cf32(mixed)
            corr = self.inv.execute_cf32(
                (rx_fft * self.replica_conj).astype(np.complex64)
            )
            mag = np.abs(corr)
            i = int(np.argmax(mag))
            if best is None or mag[i] > best[0]:
                best = (mag[i], int(k), i)
        return best


def _time(fn, rx, reps=200):
    fn(rx)  # warm-up: exclude first-call cache/allocator effects
    t0 = time.perf_counter()
    for _ in range(reps):
        fn(rx)
    return (time.perf_counter() - t0) / reps


def main():
    replica = _replica()
    true_k = 5
    code_phase = 777
    rx = _rx_epoch(replica, true_k, code_phase, snr_db=10.0)

    roll_bank = RollFftBank(replica)
    mixer_bank = MixerBank(replica)

    peak_a, k_a, phase_a = roll_bank.search(rx)
    peak_b, k_b, phase_b = mixer_bank.search(rx)
    print(f"true: k={true_k} code_phase={code_phase}")
    print(f"(A) roll-FFT : k={k_a} code_phase={phase_a} peak={peak_a:.1f}")
    print(f"(B) mixer    : k={k_b} code_phase={phase_b} peak={peak_b:.1f}")
    assert (k_a, phase_a) == (true_k, code_phase), "roll-FFT bank off-target"
    assert (k_b, phase_b) == (true_k, code_phase), "mixer bank off-target"

    t_a = _time(roll_bank.search, rx)
    t_b = _time(mixer_bank.search, rx)
    print()
    print(
        f"N={N} samples/epoch, {N_BINS} frequency hypotheses "
        f"(+/-{UNCERTAINTY_HZ / 1e3:.0f} kHz @ {BIN_HZ / 1e3:.3f} kHz spacing)"
    )
    print(f"(A) roll-FFT bank : {t_a * 1e3:.3f} ms/epoch  "
          f"({t_a / N_BINS * 1e6:.2f} us/bin)")
    print(f"(B) mixer bank    : {t_b * 1e3:.3f} ms/epoch  "
          f"({t_b / N_BINS * 1e6:.2f} us/bin)")
    print(f"speedup (B/A)     : {t_b / t_a:.2f}x")


if __name__ == "__main__":
    main()
