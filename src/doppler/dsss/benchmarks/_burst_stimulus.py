"""One 64k burst stimulus, shared by the burst chain's benchmarks.

`DsssBurstReceiver` composes `BurstAcquisition` -> refine -> `BurstDemod`,
with `BurstDespreader` as the tracked alternative and
`PolynomialPhaseEstimator` as the estimator `BurstDemod` drives. All five
benchmarks want the same thing: **one 64k block that carries bursts and one
that does not**, generated the same way, so the rows are comparable across
objects and the composed object's number can be read against the sum of its
parts.

That is a helper, not five copies of a waveform builder. A private copy in
each file would drift -- and the thing it would drift on is the geometry
every row is indexed by.

Both blocks carry the same AWGN floor (``gap_noise="auto"``). An idle block
built from silence would measure a CFAR path that never sees a real
denominator, which is not the idle case a caller has.

Not a pytest fixture: `bench_*.py` files are collected individually, and a
`conftest` fixture would make the stimulus invisible at the point of use.
Import it and call it.
"""

import numpy as np

from doppler.wfm import PN, Composer, Segment

#: Small on purpose: several bursts fit one 64k block and the benchmarks run
#: in seconds. Throughput is what these files measure; sensitivity is the
#: certification's job.
#:
#: **The acquisition length is chosen on the TRANSFORM, not just the code.**
#: `acq` calls `fft_create(sf * spc)` verbatim -- the code-axis correlation is
#: circular, so it cannot pad to a friendlier length without changing the
#: correlation. A 127-chip m-sequence at spc=4 gives 508 = 2^2 * 127, and
#: pocketfft falls to Bluestein on the prime: 9.70 us against 0.75 us for
#: 512, and 21 MSa/s against 52 end-to-end. 255 * 2 = 510 = 2 * 3 * 5 * 17 is
#: smooth AND keeps the ideal m-sequence autocorrelation (ratio 255 against
#: the 127 the old shape had) -- better on both axes, not a trade.
#:
#: Not rounded to 512: no binary code of that length has good periodic
#: autocorrelation (an extended m-sequence gives 8.0, a 4000-code random
#: search reached 10.7), and the certification brackets that -- ratio 31
#: found every burst offset, 1.07 lost 47%.
ACQ_SF_BITS, DATA_SF_BITS = 8, 5  # -> 255- and 31-chip m-sequences
REPS, SPC, PAYLOAD = 5, 2, 64  # spc >= 2 always
CHIP_RATE = 1.0e6
ESN0_DB = 12.0
BLOCK_64K = 65_536
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], dtype=np.uint8)

ACQ_SF = 2**ACQ_SF_BITS - 1
DATA_SF = 2**DATA_SF_BITS - 1
FRAME_SYMS = len(SYNC) + PAYLOAD + 16  # sync | payload | CRC-16
#: What the receiver hands back per burst — it stops at decisions, so the
#: frame comes back whole and the payload is a slice at `len(SYNC)`.
PAYLOAD_OFF = len(SYNC)
#: One burst's active span, in samples. Derived from the geometry above so
#: it cannot disagree with it.
BURST_LEN = (REPS * ACQ_SF + FRAME_SYMS * DATA_SF) * SPC


def mls(n_stages, seed):
    """An m-sequence from doppler's own PN generator, not a random array.

    Not cosmetic for a benchmark: a code without the -1 sidelobe property
    lets the CFAR reference read the code's own autocorrelation instead of
    the noise, which changes how often the detection stages run -- and so
    changes what is being measured. The certification put a number on it:
    53% of burst offsets detected against 100%.
    """
    n = 2**n_stages - 1
    return (
        np.asarray(PN(poly=0, seed=seed, length=n_stages).generate(n)) & 1
    ).astype(np.uint8)


def _segment(acq_code, data_code, payload, k, off):
    return Segment(
        type="dsss",
        fs=CHIP_RATE * SPC,
        freq=0.0,
        snr=ESN0_DB,
        snr_mode="esno",  # Es/N0 of the DATA_SF-chip data symbol
        seed=k + 1,
        sps=SPC,  # samples per CHIP
        acq_code=acq_code.tobytes(),
        acq_reps=REPS,
        data_code=data_code.tobytes(),
        sync=SYNC.tobytes(),
        payload=payload.tobytes(),  # CRC-16 auto-appended
        gap_noise="auto",
        off_samples=off,
    )


def burst_stimulus():
    """``(acq_code, data_code, payload, bursts, idle)``.

    ``bursts`` is 64k carrying ``N_BURSTS`` complete bursts; ``idle`` is 64k
    of the same noise floor with none. Both are exactly ``BLOCK_64K``
    samples, so a rate computed from either is indexed by the same
    denominator.

    **The burst spacing is read from the receiver, not computed here.**
    Detections closer together than `refine_span` are coalesced as one
    preamble, so a stimulus that packs them tighter silently measures fewer
    bursts than it claims: at spacing exactly `refine_span` one of four
    bursts is lost, and one sample more recovers it. `refine_span` was
    internal until gh-1011 -- the alternative was restating
    ``(4*reps + 4) * code_period`` here, which is the drift this repo
    forbids and which the object's OWN header got wrong (it documented
    ``2*reps*code_period``, 2.4x low).
    """
    acq_code = mls(ACQ_SF_BITS, seed=1)
    data_code = mls(DATA_SF_BITS, seed=3)
    payload = np.random.default_rng(0).integers(0, 2, PAYLOAD).astype(np.uint8)

    spacing, n_bursts = packing(acq_code, data_code)

    # The LAST segment's gap is a whole block, so slicing to BLOCK_64K
    # cannot truncate a burst -- every one of the N inside is complete.
    bursts = Composer(
        [
            _segment(
                acq_code,
                data_code,
                payload,
                k,
                spacing - BURST_LEN if k < n_bursts - 1 else BLOCK_64K,
            )
            for k in range(n_bursts)
        ]
    ).compose()[:BLOCK_64K]

    # Idle: the same generator and floor, read past the burst it made.
    idle = Composer(
        [_segment(acq_code, data_code, payload, 0, BLOCK_64K * 2)]
    ).compose()[-BLOCK_64K:]

    assert bursts.size == idle.size == BLOCK_64K
    return acq_code, data_code, payload, bursts, idle


def packing(acq_code, data_code):
    """``(spacing, n_bursts)`` for one 64k block, from the receiver itself.

    One sample past `refine_span` is enough to keep two detections distinct
    -- measured -- but a margin costs nothing here and a stimulus sitting on
    an inequality is a stimulus that breaks on a geometry change.
    """
    from doppler.dsss import DsssBurstReceiver

    probe = DsssBurstReceiver(
        acq_code=acq_code,
        data_code=data_code,
        sync=SYNC,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=FRAME_SYMS,
        cn0_dbhz=60.0,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=0.9,
        carrier_hz=0.0,
        max_rate=0.0,
        est_segments=10,
    )
    spacing = probe.refine_span + probe.refine_span // 10
    return spacing, BLOCK_64K // spacing


def rate(benchmark):
    """Record MSa/s from the MINIMUM round, not the mean.

    The minimum is the least-disturbed observation; a mean folds in every
    scheduler hiccup the machine had. `docs/dev/contributing/benchmarking.md`
    is the standing rule this follows.
    """
    if benchmark.stats:
        benchmark.extra_info["MSa_s"] = (
            BLOCK_64K / benchmark.stats["min"] / 1e6
        )
