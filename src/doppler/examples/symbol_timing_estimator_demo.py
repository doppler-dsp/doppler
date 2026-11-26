"""Symbol timing from one block, with no loop.

A timing loop walks its error out, and how long that takes depends on where
it started. :class:`doppler.track.SymbolTimingEstimator` does not walk: it
reads the timing off one block of matched-filtered samples, in the block's
own length, whatever the timing was. It uses only the stream's power, so it
works before the carrier is recovered.

This script makes a BPSK stream with a carrier offset and noise, matched
filters it, and asks the estimator where the eye is. It then checks the
answer three ways, and exits non-zero if any fails:

- sampling at the estimate gives more power than sampling half a symbol
  away (the eye is where it says);
- the same stream on a different carrier gives the same timing;
- a second block gives the transmitter's clock offset;
- :class:`doppler.track.MatchedSymbolTimingEstimator`, which carries the
  matched filter itself, names the eye on the timeline of the unfiltered
  samples: the same stream one sample later moves its answer by one sample.
"""

# --8<-- [start:signal]
import numpy as np

from doppler.resample import MatchedRateConverter, Resampler
from doppler.track import MatchedSymbolTimingEstimator, SymbolTimingEstimator
from doppler.wfm import Synth

SPS = 4.0  # samples per symbol, at the matched filter's output
BETA = 0.35  # root-raised-cosine roll-off
SPAN = 8  # its one-sided span, symbols
# Symbols in a block. Long enough to average the noise, and short enough
# that the clock offset moves the timing well under half a symbol across it
# (CLOCK_PPM * NSYM = 0.31 symbols), which is the estimator's own limit.
NSYM = 1024
ES_N0_DB = 10.0
CLOCK_PPM = 300.0  # how fast the transmitter's symbol clock runs


def transmit(nsym, carrier, es_n0_db=ES_N0_DB, seed=1):
    """``nsym`` BPSK symbols on a carrier of ``carrier`` cycles a sample.

    The library's generator shapes the symbols, mixes the carrier and adds
    the noise for ``es_n0_db`` (100 or more is no noise). The resampler
    then shortens every symbol to ``SPS / (1 + CLOCK_PPM * 1e-6)`` samples,
    which is a transmitter whose symbol clock runs fast against the
    receiver's.
    """
    rng = np.random.default_rng(seed)
    syms = np.where(rng.integers(0, 2, nsym) > 0, 1.0, -1.0)
    gen = Synth(
        type="symbols",
        symbols=syms.astype(np.complex64),
        pulse="rrc",
        rrc_beta=BETA,
        rrc_span=SPAN,
        sps=int(SPS),
        fs=1.0,
        freq=carrier,
        snr=es_n0_db,
        snr_mode="esno",
        seed=seed,
    )
    x = np.asarray(gen.steps(nsym * int(SPS))).astype(np.complex64)
    fast = Resampler(rate=1.0 / (1.0 + CLOCK_PPM * 1e-6))
    return np.asarray(fast.execute(x))


def matched(x):
    """The matched filter: a unit-rate converter carrying the pulse."""
    mf = MatchedRateConverter(
        rate=1.0, pulse="rrc", beta=BETA, span=SPAN, pulse_sps=SPS
    )
    return np.asarray(mf.execute(x))


# Two blocks of one stream, on a carrier 2% of the sample rate off.
y = matched(transmit(2 * NSYM, carrier=0.02))
half = y.size // 2

est = SymbolTimingEstimator(sps=SPS)
first = est.estimate(y[:half])
second = est.estimate(y[half:])
# --8<-- [end:signal]

print(
    f"first block:  tau {first.tau:+.4f} symbols, quality {first.quality:.3f}"
)
print(
    f"second block: tau {second.tau:+.4f} symbols, "
    f"rate {second.rate * 1e6:+.0f} ppm (sent {CLOCK_PPM:+.0f} ppm fast)"
)

# 1. The eye is where it says. The estimator's clock counts samples from its
#    first one, so the eye centers of the first block are the samples whose
#    clock phase is nearest `tau`.
phase = (np.arange(half) / SPS) % 1.0
dist = np.abs((phase - first.tau + 0.5) % 1.0 - 0.5)
at_eye = np.mean(np.abs(y[:half][dist < 0.5 / SPS]) ** 2)
off_eye = np.mean(np.abs(y[:half][dist > 0.5 - 0.5 / SPS]) ** 2)
print(f"power at the estimate {at_eye:.3f}, half a symbol away {off_eye:.3f}")
# A root-raised cosine of this roll-off keeps most of its power between the
# eye centers too, so the difference is modest; its sign is the check.
assert at_eye > off_eye, "the estimate is not at the eye center"

# 2. The carrier does not enter the estimator. With the noise off, the
#    same symbols on two carriers 5% of the sample rate apart give the same
#    timing to a few thousandths of a symbol. What is left is not the
#    estimator's: the matched filter ahead of it sees a pulse shifted in
#    frequency, and passes a slightly different shape.
clean = [
    SymbolTimingEstimator(sps=SPS)
    .estimate(matched(transmit(2 * NSYM, carrier=f, es_n0_db=100.0))[:half])
    .tau
    for f in (0.02, -0.03)
]
print(f"no noise, two carriers: tau {clean[0]:+.4f} and {clean[1]:+.4f}")
assert abs(clean[0] - clean[1]) < 5e-3, "the timing moved with the carrier"

# 3. Two blocks give the clock offset. A fast transmitter's symbols are
#    short, so the eye arrives earlier each symbol and the rate is negative.
assert second.has_rate == 1
sent = -CLOCK_PPM * 1e-6
assert abs(second.rate - sent) < 1e-4, "the rate is not the clock offset"


# 4. With a matched filter of its own. The matched class takes the samples
#    as they arrive and says where the eye is among THEM: `eye` is a sample
#    index into what it was handed, with its filter's delay taken out. The
#    same stream handed over one sample later has to answer one sample
#    later.
# --8<-- [start:matched]
def own_filter(x):
    te = MatchedSymbolTimingEstimator(
        sps=SPS, pulse="rrc", beta=BETA, span=SPAN
    )
    return te.estimate(x)


x = transmit(NSYM, carrier=0.02)
on_time = own_filter(x)
late = own_filter(np.concatenate([np.zeros(1, np.complex64), x]))
# --8<-- [end:matched]
moved = (late.eye - on_time.eye + SPS / 2) % SPS - SPS / 2
print(
    f"own filter: eye at sample {on_time.eye:.2f} (and every {SPS:.0f} "
    f"from it); one sample late moves it {moved:+.3f}"
)
assert abs(moved - 1.0) < 0.05, "the eye is not on the input's timeline"
print("PASS")
