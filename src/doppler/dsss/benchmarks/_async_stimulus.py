"""One continuous async-DSSS stream, shared by the continuous receivers' rows.

`DsssReceiver` and `AsyncDsssReceiver` both acquire a code phase they have
not been told and then track an asynchronous data stream riding it, so both
want the same thing: **contiguous 64k blocks of one long capture**, generated
by the shipped waveform engine, with the transmitted bits kept so a row can
prove it still decodes. One helper, not two copies of a waveform builder that
would drift on the geometry every row is indexed by — the same reason the
burst chain shares `_burst_stimulus.py`.

The operating point is the one `src/doppler/dsss/tests/test_dsss_receiver.py`
validates: a 1023-chip Gold code at 3 Mcps, 2 samples per chip, 2100 data
symbols per second (non-integer chips per symbol — that is what "async"
means), a +50 Hz residual. Clean enough that every row decodes bit-exactly:
noise changes what a receiver concludes, not what it costs, and sensitivity
is the certification's job.

Not a pytest fixture, for the reason `_burst_stimulus.py` gives: import it
and call it, so the stimulus is visible at the point of use.
"""

import numpy as np

from doppler.ber import BerMeter
from doppler.wfm import Gold, Synth

SF = 1023
CHIP_RATE = 3.0e6
SYM_RATE = 2100.0
SPC = 2
FS = CHIP_RATE * SPC
DOPPLER_HZ = 50.0
ESN0_DB = 40.0
BLOCK_64K = 65_536
#: Samples per data symbol: ~2857, deliberately not an integer.
TSYM = FS / SYM_RATE

CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)


def stream(n_blocks, seed=6):
    """``(x, bits)``: `n_blocks` contiguous 64k blocks and the bits they carry.

    ``x`` is one continuous capture from `Synth` (``type="dsss"`` with
    ``symbol_rate`` set selects the continuous asynchronous mode), so block
    ``k`` is ``x[k * BLOCK_64K : (k + 1) * BLOCK_64K]`` and consecutive blocks
    join with no seam. ``bits`` is the data, one bit per symbol, in the order
    the engine spreads it.
    """
    n = n_blocks * BLOCK_64K
    nsym = int(np.ceil(n / TSYM)) + 4
    bits = np.random.default_rng(seed).integers(0, 2, nsym).astype(np.uint8)
    x = Synth(
        type="dsss",
        fs=FS,
        sps=SPC,  # samples per CHIP
        data_code=CODE.tobytes(),
        symbol_rate=SYM_RATE,
        freq=DOPPLER_HZ,
        snr=ESN0_DB,
        snr_mode="esno",
        seed=seed,
        data=bits,
    ).steps(n)
    x = np.ascontiguousarray(np.asarray(x).astype(np.complex64))
    assert x.size == n, "the blocks are the denominator; they must be full"
    return x, bits


def bit_errors(syms, bits, n_fed):
    """``(errors, scored)`` of recovered BPSK symbols against the sent bits.

    ``n_fed`` is how many samples of the stream the receiver was actually
    given — not the stream's length, which a disabled benchmark (one call
    instead of every round) does not reach.

    Through `BerMeter`, the library's own meter: the alignment is DETECTED by
    correlating a marker of the truth against the symbols (lag and absolute
    phase both), never searched by minimising the error count.

    **The marker is taken from the middle of what was decoded**, not from
    the start of the truth: a receiver that locks part-way through a capture
    starts emitting at a symbol index nobody told it, so ``truth[:64]`` may
    never have been decoded at all. Its output does END where its input
    ends, so the ``syms.size`` truth symbols before ``n_fed / TSYM`` are what
    it covers, and the marker sits halfway into them. The lag span is the
    whole truth, so an estimate of that start which is off by a few symbols
    costs nothing.
    """
    met = BerMeter(m=2)
    met.set_truth(bits)
    t0 = max(0, int(n_fed / TSYM) - syms.size // 2)
    if not met.align(syms, t0=t0, n_marker=64, lag_span=bits.size):
        return None, 0
    scored = met.score(syms, lo=0, hi=syms.size)
    return met.errors, scored
