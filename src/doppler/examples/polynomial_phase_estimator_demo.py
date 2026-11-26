"""Carrier frequency and its rate from one block, with no loop.

A burst from a fast-moving transmitter arrives with a Doppler offset that
is itself moving. :class:`doppler.estimation.PolynomialPhaseEstimator`
reads both off one block of samples whose modulation has been stripped:
it dechirps the block at each of a bank of rate hypotheses, takes an FFT
of each, and reports the peak of that surface.

This script builds an unmodulated preamble with a known offset and rate
under noise, estimates both, and checks the answer the way a receiver
would use it, exiting non-zero if either check fails:

- taking the estimated phase off the block leaves a carrier that adds up
  coherently: most of the block's signal amplitude is recovered, where the
  block as it arrived recovers almost none;
- a search with the rate held at zero cannot do that on this block, which
  is what the rate search is for.
"""

# --8<-- [start:estimate]
import numpy as np

from doppler.estimation import PolynomialPhaseEstimator

N = 2048  # samples in the preamble
FREQ = 0.0371  # carrier offset, cycles a sample
RATE = 2.0e-5  # its rate, cycles a sample squared: 0.04 cycles/sample over N
SNR_DB = 0.0  # a sample's signal-to-noise ratio

m = np.arange(N)
rng = np.random.default_rng(7)
noise = (rng.standard_normal(N) + 1j * rng.standard_normal(N)) * np.sqrt(
    0.5 * 10 ** (-SNR_DB / 10)
)
x = (np.exp(2j * np.pi * (FREQ * m + 0.5 * RATE * m * m)) + noise).astype(
    np.complex64
)

est = PolynomialPhaseEstimator(max_len=N, max_rate=5e-5).estimate(x)
# --8<-- [end:estimate]
print(
    f"sent      {FREQ:.5f} cycles/sample, {RATE:.2e} cycles/sample^2\n"
    f"estimated {est.freq_norm:.5f} cycles/sample, "
    f"{est.rate_norm:.2e} cycles/sample^2 ({est.snr_db:.1f} dB)"
)


def recovered(freq, rate):
    """Share of the signal's amplitude left after taking a phase off.

    A carrier whose phase has been removed exactly sums to ``N``; one that
    still turns inside the block sums to less. The noise adds about
    ``1 / sqrt(N)`` either way.
    """
    model = np.exp(-2j * np.pi * (freq * m + 0.5 * rate * m * m))
    return abs(np.sum(x * model)) / N


got = recovered(est.freq_norm, est.rate_norm)
raw = recovered(0.0, 0.0)
print(f"amplitude recovered: {got:.3f} with the estimate, {raw:.3f} without")
assert got > 0.9, "the estimate does not line the block up"
assert raw < 0.1, "the block was coherent to begin with: the check is empty"

# With the rate search off the best single frequency still leaves the
# carrier turning through 0.04 cycles a sample across the block.
flat = PolynomialPhaseEstimator(max_len=N, max_rate=0.0).estimate(x)
no_rate = recovered(flat.freq_norm, 0.0)
print(f"amplitude recovered with no rate search: {no_rate:.3f}")
assert no_rate < 0.5 * got, "the rate search made no difference"
print("PASS")
