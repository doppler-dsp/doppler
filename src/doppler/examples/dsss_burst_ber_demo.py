"""dsss_burst_ber_demo.py — a DSSS burst's BER vs Eb/N0, through ``Plan``.

The last step of the burst walkthrough: the same kind of burst
`dsss_burst_receiver_demo.py` decodes once, now decoded hundreds of times
across an Eb/N0 sweep and scored against theory. Three objects, each doing
the one job it owns:

- ``doppler.wfm.Plan`` renders the burst's signal ONCE and hands back a
  fresh noise draw per ``plan.at(snr, seed)`` -- the Monte-Carlo stimulus;
- ``doppler.dsss.DsssBurstReceiver`` finds the burst blind and decides its
  bits, exactly as it would on a capture;
- ``doppler.ber.BerMeter`` turns the error count into an exact interval,
  following `ber_awgn_demo.py`'s method: stop on ERRORS, assert on the
  interval's limits, never on the point estimate.

The payload is PN data and nothing else -- no CRC, no frame description.
The transmitted bits and the scoring truth come from ONE Field string,
through ``doppler.wfm.field_bits``, so they cannot disagree. What is
measured is a bit error rate; with no framing there is no frame error rate
to quote.

Limits, stated rather than hidden:

- the payload is the SAME in every trial: a Plan caches the signal, and
  only the noise (and a ranged gap, which this scene does not use) varies
  per draw. A fixed PN payload is a fair BER stimulus for a linear BPSK
  receiver; it is not a random-data one;
- the BER is conditional on acquisition. A trial whose burst the receiver
  does not find at its true start contributes no bits, and is counted and
  printed as a miss rather than silently folded in;
- the BPSK sign is resolved by the receiver's documented minimal
  configuration: a 13-symbol sync word ahead of the payload
  (``burst_demod_core.h``: "the sync word, to find the frame and resolve
  the BPSK sign"). No truth is used to fix the sign. The truth IS used to
  pick which decoded window to score -- the one at the burst's known start
  -- which is what makes a missed burst a miss and not a pile of errors.

Run:  python -m doppler.examples.dsss_burst_ber_demo [out.png] [--full]

``--full`` is the one-time measurement the stated margin rests on (1000
errors per point, 0-8 dB, about 200 s); the default is the short
curve the gallery and the example gate run.
"""

from __future__ import annotations

import sys
import time

import numpy as np

from doppler.ber import BerMeter, ber_esn0_db_for_ser, ber_theory_ber
from doppler.dsss import DsssBurstReceiver
from doppler.wfm import Composer, field_bits, prepare

FULL = "--full" in sys.argv

# ── the burst ───────────────────────────────────────────────────────────────
# Every sequence is a Field, and every Field goes through the one parser.
# The payload's Field is the TRUTH as well as the transmit bits.
ACQ_FIELD = "pn:255:8:1"  # preamble: a 255-chip m-sequence, repeated
DATA_FIELD = "pn:31:5:3"  # the payload spreading code (processing gain)
PAYLOAD_FIELD = "pn:1023:10:1"  # 1023 PN data bits per burst
SYNC = np.array([0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0], np.uint8)  # B-13

ACQ, DATA, PAYLOAD = (
    field_bits(f) for f in (ACQ_FIELD, DATA_FIELD, PAYLOAD_FIELD)
)
REPS, SPC, CHIP_RATE = 5, 2, 1.0e6
FS = CHIP_RATE * SPC
SF = len(DATA)  # chips per data bit: the processing gain, 10log10(31)
FRAME_SYMS = len(SYNC) + len(PAYLOAD)  # what push() returns per burst
LEAD = 4000  # noise ahead of the burst: its true start, in samples

SAMPLES_PER_BIT = SF * SPC  # fs / Rb: one data bit spans SF chips
RB = CHIP_RATE / SF  # data bit rate, bit/s

# ── the sweep ───────────────────────────────────────────────────────────────
if FULL:
    EBN0_DB = np.arange(0.0, 9.0, 1.0)
    TARGET_ERRORS = 1000
else:
    EBN0_DB = np.array([0.0, 2.0, 4.0, 6.0, 7.0])
    TARGET_ERRORS = 100  # ~10% relative standard error, 1/sqrt(r)
# Errors inside one burst share that burst's phase estimate, so they are not
# independent. A floor on BURSTS per point keeps a high-BER point from being
# 100 errors out of one or two bursts, where the binomial interval would be
# narrower than the truth.
MIN_TRIALS = 20
# The trial budget is ten times what ideal BPSK needs to reach the target.
# A point that exhausts it has not resolved its rate and is NOT quoted --
# and a receiver (or an Eb/N0 axis) that needs ten times theory's trials is
# broken, so the run stops and fails instead of grinding on.
BUDGET = 10
CONF = 0.99
# Acquisition is designed 3 dB below the sweep's lowest C/N0, so a missed
# burst is rare across the whole curve: C/N0 = Eb/N0 + 10 log10(Rb).
CN0_DESIGN_DBHZ = EBN0_DB[0] + 10.0 * np.log10(RB) - 3.0
PD = 0.999


def receiver() -> DsssBurstReceiver:
    return DsssBurstReceiver(
        acq_code=ACQ,
        data_code=DATA,
        sync=SYNC,
        reps=REPS,
        spc=SPC,
        chip_rate=CHIP_RATE,
        frame_syms=FRAME_SYMS,
        cn0_dbhz=CN0_DESIGN_DBHZ,
        doppler_uncertainty=0.0,
        pfa=1e-3,
        pd=PD,
        carrier_hz=0.0,
        max_rate=0.0,
        est_segments=10,
    )


# --8<-- [start:margin]
def sync_phase_limited_ber(ebn0_db: float) -> float:
    """BPSK BER for THIS receiver's carrier-phase estimate, not a fit.

    BurstDemod is feedforward: the residual carrier phase comes from the
    complex peak of the sync-word correlation (burst_demod_core.h, step 4),
    a sum over len(SYNC) = 13 despread symbols each at Es/N0 = Eb/N0. That
    estimate's error is Gaussian with variance 1 / (2 * 13 * Eb/N0), and a
    phase error phi scales the decision amplitude by cos(phi), so the
    expected BER is  E_phi[ Q(sqrt(2 Eb/N0) cos phi) ].  Averaged by
    Gauss-Hermite quadrature over doppler's own Q, it is ideal BPSK plus
    the loss this estimator must cost: ~0.2 dB at 0 dB, falling as 1/Eb/N0.
    """
    g = 10.0 ** (ebn0_db / 10.0)
    sigma = np.sqrt(1.0 / (2.0 * len(SYNC) * g))
    x, w = np.polynomial.hermite_e.hermegauss(40)
    ber = [ber_theory_ber(2, g * np.cos(sigma * xi) ** 2) for xi in x]
    return float(np.dot(w, ber) / w.sum())


# --8<-- [end:margin]

# ── the scene, and the Plan over it ─────────────────────────────────────────
# --8<-- [start:plan]
BASE_EBN0_DB = 6.0
rx = receiver()  # ONE receiver for the whole sweep: reset() between trials
# One dsss source is the whole scene, so the Composer takes it directly.
# snr_mode="ebno" makes `snr` the Eb/N0 of one data bit (a dsss source's
# symbol is the despread data bit), and Plan's at() takes the SAME number:
# no hand conversion to the over-fs SNR anywhere below.
scene = Composer(
    type="dsss",
    fs=FS,
    snr=BASE_EBN0_DB,
    snr_mode="ebno",
    seed=11,
    sps=SPC,  # samples per CHIP
    acq_code=ACQ,
    acq_reps=REPS,
    data_code=DATA,
    sync=SYNC,
    data=PAYLOAD,  # PN data, and nothing after it
    crc="none",
    delay_samples=LEAD,
    # the receiver holds a burst until it has seen refine_span past its
    # end, so that much trailing noise is what lets it emit
    off_samples=rx.refine_span,
    gap_noise="auto",
)
plan = prepare(scene)  # the burst's signal is rendered ONCE, here

# The anchor contract: at the scene's own Eb/N0 and the anchor seed, a Plan
# draw IS a full compose of the same scene, byte for byte.
anchor = plan.at(BASE_EBN0_DB, plan.anchor_seed)
assert anchor.tobytes() == scene.compose().tobytes(), (
    "plan.at(base_ebn0, anchor_seed) is not byte-identical to compose()"
)
# --8<-- [end:plan]

# --8<-- [start:reuse]
# Building a receiver solves its detection design (the Pd/Pfa threshold)
# every time; reset() keeps the design and clears the stream. A reused
# receiver must decode a trial exactly as a fresh one does -- pinned here,
# on a draw pushed AFTER the reused one has already decoded another.
rx.push(plan.at(BASE_EBN0_DB, 1))
rx.reset()
x = plan.at(BASE_EBN0_DB, 2)
fresh = receiver()
assert np.array_equal(np.asarray(rx.push(x)), np.asarray(fresh.push(x)))
assert rx.events().tobytes() == fresh.events().tobytes(), (
    "a reset() receiver's events differ from a fresh receiver's"
)
rx.reset()
# --8<-- [end:reuse]


# --8<-- [start:measure]
def measure(ebn0_db: float, seed0: int, meter: BerMeter) -> dict:
    """One Eb/N0 point: trials until TARGET_ERRORS (and MIN_TRIALS bursts).

    Each trial is a fresh noise draw of the same cached burst, pushed whole
    through the one receiver, reset between trials (output-identical to a
    fresh one, asserted above). The window scored is the one the receiver
    decoded at the burst's true start; the payload is the frame after the
    sync word, compared bit for bit with the Field's own bits.
    """
    ideal = ber_theory_ber(2, 10.0 ** (ebn0_db / 10.0))
    max_trials = max(
        MIN_TRIALS, int(BUDGET * TARGET_ERRORS / (ideal * PAYLOAD.size))
    )
    errors = bits = misses = trials = 0
    draws = set()  # every trial's noise must be its own
    while (errors < TARGET_ERRORS or trials < MIN_TRIALS) and (
        trials < max_trials
    ):
        x = plan.at(ebn0_db, seed0 + trials)
        trials += 1
        draws.add(hash(x.tobytes()))
        rx.reset()
        out = np.asarray(rx.push(x))
        hit = [
            k
            for k, e in enumerate(rx.events())
            if int(e["preamble_start"]) == LEAD
        ]
        if not hit:
            misses += 1
            continue
        frame = out[hit[0] * FRAME_SYMS :][:FRAME_SYMS]
        got = frame[len(SYNC) :]
        errors += int(np.count_nonzero(got != PAYLOAD))
        bits += PAYLOAD.size
    return {
        "ebn0": ebn0_db,
        "trials": trials,
        "misses": misses,
        "distinct": len(draws),
        "ci": meter.interval(errors, bits),
        "quoted": errors >= TARGET_ERRORS and trials >= MIN_TRIALS,
    }


# --8<-- [end:measure]


def main(out: str = "dsss_burst_ber_demo.png") -> None:
    meter = BerMeter(m=2, conf=CONF)
    t0 = time.perf_counter()
    points = [
        measure(float(eb), 1000 + 100_000 * i, meter)
        for i, eb in enumerate(EBN0_DB)
    ]
    elapsed = time.perf_counter() - t0

    print(
        f"DSSS burst: {len(ACQ)}-chip preamble x{REPS}, SF {SF}, "
        f"{PAYLOAD.size} PN bits ({PAYLOAD_FIELD}), {SPC} samples/chip, "
        f"Rb {RB / 1e3:.2f} kbit/s"
    )
    print(
        f"snr_mode=ebno: {SAMPLES_PER_BIT} samples per data bit; "
        f"acquisition designed at {CN0_DESIGN_DBHZ:.1f} dB-Hz, pd {PD}"
    )
    print(
        f"{'Eb/N0':>6} {'trials':>6} {'miss':>4} {'errors':>6} "
        f"{'BER':>9} {'99% interval':>21} {'theory':>9} {'sync-lim':>9} "
        f"{'loss':>7}"
    )
    # --8<-- [start:assert]
    quoted = 0
    for p in points:
        ci, eb = p["ci"], p["ebn0"]
        ideal = ber_theory_ber(2, 10.0 ** (eb / 10.0))
        limited = sync_phase_limited_ber(eb)
        # Stated only when the point stopped on errors: a point that ran
        # out of trials has not resolved its rate and is not quoted.
        loss = eb - ber_esn0_db_for_ser(2, ci.p_hat) if p["quoted"] else None
        print(
            f"{eb:6.1f} {p['trials']:6d} {p['misses']:4d} {ci.errors:6d} "
            f"{ci.p_hat:9.3e} [{ci.lo:9.3e}, {ci.hi:9.3e}] {ideal:9.3e} "
            f"{limited:9.3e} "
            + (f"{loss:+6.2f}dB" if loss is not None else "  (n/q)")
        )
        # Independent Monte Carlo: every trial drew its own noise.
        assert p["distinct"] == p["trials"], (
            f"{eb} dB: {p['trials']} trials but {p['distinct']} distinct "
            "draws -- the Plan's seed is not varying the noise"
        )
        # Acquisition met its design: the miss rate's interval admits 1-pd.
        miss = meter.interval(p["misses"], p["trials"])
        assert miss.lo <= 1.0 - PD, (
            f"{eb} dB: {p['misses']}/{p['trials']} bursts missed, more than "
            f"a pd={PD} acquisition allows"
        )
        quoted += bool(p["quoted"])
        # Both limits hold for EVERY point, quoted or not: an unresolved
        # point's interval is wide, not wrong. The receiver cannot beat
        # coherent BPSK ...
        assert ci.hi >= ideal, (
            f"{eb} dB: BER interval [{ci.lo:.3e}, {ci.hi:.3e}] lies below "
            f"ideal BPSK {ideal:.3e} -- the Eb/N0 axis is wrong"
        )
        # ... and must be no worse than its own sync-phase estimate costs.
        assert ci.lo <= limited, (
            f"{eb} dB: BER interval [{ci.lo:.3e}, {ci.hi:.3e}] lies above "
            f"the sync-phase-limited {limited:.3e} -- a loss the receiver's "
            "phase estimate does not account for"
        )
    assert quoted == len(points), (
        f"only {quoted}/{len(points)} points reached {TARGET_ERRORS} errors"
    )
    # --8<-- [end:assert]
    total = sum(p["trials"] for p in points)
    print(
        f"{total} bursts in {elapsed:.1f} s "
        f"({elapsed / total * 1e3:.1f} ms per render + decode)"
    )

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fine = np.linspace(EBN0_DB[0] - 0.5, EBN0_DB[-1] + 0.5, 200)
    fig, ax = plt.subplots(figsize=(7.2, 4.8))
    ax.semilogy(
        fine,
        [ber_theory_ber(2, 10.0 ** (d / 10.0)) for d in fine],
        color="0.35",
        lw=1.2,
        label="coherent BPSK, $Q(\\sqrt{2E_b/N_0})$",
    )
    ax.semilogy(
        fine,
        [sync_phase_limited_ber(d) for d in fine],
        color="tab:orange",
        lw=1.0,
        ls="--",
        label="with a 13-symbol sync phase estimate",
    )
    ebs = [p["ebn0"] for p in points]
    ber = np.array([p["ci"].p_hat for p in points])
    lo = ber - np.array([p["ci"].lo for p in points])
    hi = np.array([p["ci"].hi for p in points]) - ber
    ax.errorbar(
        ebs,
        ber,
        yerr=[lo, hi],
        fmt="o",
        ms=5,
        capsize=3,
        color="tab:blue",
        label=f"DsssBurstReceiver, {TARGET_ERRORS} errors/point "
        f"(99% interval)",
    )
    ax.set_xlabel("$E_b/N_0$ per data bit (dB)")
    ax.set_ylabel("bit error rate")
    ax.set_title(
        f"DSSS burst BER through one Plan: {total} bursts, "
        f"SF {SF}, {PAYLOAD.size} PN bits each",
        fontsize=10,
    )
    ax.grid(alpha=0.3, which="both")
    ax.legend(fontsize=8, loc="lower left")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"wrote {out}")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--full"]
    main(args[0] if args else "dsss_burst_ber_demo.png")
