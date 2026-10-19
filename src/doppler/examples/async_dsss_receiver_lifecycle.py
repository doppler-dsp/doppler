"""async_dsss_receiver_lifecycle.py -- one
:class:`~doppler.dsss.AsyncDsssReceiver` through a signal's life.

The receiver follows one continuous DSSS emitter from the first code epoch it
searches to the moment the emitter leaves. It searches, refines the carrier
estimate, and tracks the emitter. When the emitter goes, both of its lock
detectors fall. Once they have been down for longer than a confirm
interval, the receiver moves to ``lost``. The caller resets it, and it
acquires the same emitter again when the emitter returns.

The receiver is fed one code epoch per push, with its telemetry attached. It
reports its own state, its code and symbol lock decisions, its lock metric,
the symbols it emits and the Doppler it handed over, and a lossless capture
keeps every record; the checks and the figure read only those records. Four
things about the states show in the run:

- **Searching and refining emit nothing.** The first symbols come once the
  receiver is tracking.
- **Lock-up time and fade survival vary by draw.** This run keeps tracking
  and the symbol lock through a 10 ms fade. The measurement record, under its
  own conditions, says how often a fade is ridden out; this setup is not that
  measurement.
- **The release rule is armed.** ``lost_confirm_s`` is the interval both lock
  detectors must stay down before the receiver gives up on the emitter.
- **Reset returns the receiver to searching.** A receiver that has locked
  cannot be reset onto the same signal; it searches again, and a returning
  emitter is acquired afresh.

When the receiver does not behave as this run does, read its probes in this
order: ``rx.state`` (searching, refining, tracking, lost), then
``rx.code.locked`` (the code loop's detector), then ``rx.sym.locked`` (the
symbol lock), then ``rx.sym.lock``, the metric the lock flag thresholds.
Searching that never ends means no detection at this uncertainty. Refining
that never ends means the carrier estimate has not converged, and
``rx.handover_hz`` says what it settled on. Tracking with ``rx.sym.locked``
low means the symbol lock has not come up yet, and lock-up time varies by
draw.

The design, and the reasons for the release rule, are in
``docs/design/async-dsss-receiver.md``.

Run:  python -m doppler.examples.async_dsss_receiver_lifecycle  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.source import AWGN
from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

# ── Setup: this scenario's numbers ─────────────────────────────────────────
# The caller knows the sample rate, the chip and symbol rates, and the carrier
# uncertainty it has. The receiver takes those, and the C/N0 it needs to size
# its detectors.
SF = 1023  # Gold code period, chips (CCSDS)
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # sample rate, 6.138 MSa/s
SYM_RATE = 2700.0  # BPSK symbols per second, asynchronous to the code
TE = SF * SPC  # one code epoch, samples: one push
EPOCH_S = TE / FS  # seconds of signal per push
CARRIER = 1500.0  # Hz, the emitter's carrier offset while on the air
CARRIER_UNCERTAINTY = 5000.0  # Hz, one-sided: the caller's own bound on it
ESN0_DB = 10.0  # Es/N0 of the emitter at the receiver input
CN0_DBHZ = ESN0_DB + 10.0 * np.log10(SYM_RATE)  # C/N0, dB-Hz
PFA = 1e-3  # the detectors' false-alarm rate
LOST_CONFIRM_S = 0.1  # both lock detectors down this long before LOST
SEED_EMITTER = 0  # the synth's PN and payload draw
SEED_NOISE = 2  # the noise draw

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: The timeline, in code epochs: ``"on"`` carries the signal, ``"off"`` is
#: noise alone. Written once, read by the signal, the receiver run, the checks
#: and the figure.
TIMELINE = (
    ("off", 180),  # 60 ms of noise: nothing to find yet
    ("on", 4500),  # 1.5 s: acquire, refine, track, and let the lock come up
    ("off", 30),  # 10 ms fade
    ("on", 300),  # 100 ms back on the air
    ("off", 900),  # 0.3 s: the emitter leaves, longer than the confirm time
    ("on", 4500),  # a returning emitter, on the air for 1.5 s
)


def spans():
    """Each :data:`TIMELINE` segment as ``(kind, first_epoch, end_epoch)``."""
    out, pos = [], 0
    for kind, n in TIMELINE:
        out.append((kind, pos, pos + n))
        pos += n
    return out


def make_timeline(seed: int = SEED_NOISE) -> np.ndarray:
    """The capture: an emitter switching on and off over :data:`TIMELINE`, with
    noise running throughout.

    The emitter is :class:`~doppler.wfm.Synth` in continuous DSSS mode. Its
    payload is the synth's own PRBS, so the data is not a bit array this file
    owns. Each ``"off"`` span is zeroed after synthesis, so the noise floor is
    the same before, during and after every cut. The noise is
    :class:`~doppler.source.AWGN`, with the per-component sigma that
    :func:`~doppler.wfm.wfm_awgn_amplitude` gives for the Es/N0.

    Parameters
    ----------
    seed : int, default SEED_NOISE
        The noise draw. Each draw is one realization of the same timeline; the
        lock-up time and whether a dropout holds depend on it. The C twin
        uses the same two seeds, so the default draw is the one it runs.

    Returns
    -------
    x : NDArray[np.complex64]
        The whole capture, ``sum(n for _, n in TIMELINE)`` code epochs long.
    """
    n = sum(n for _, n in TIMELINE) * TE
    sig = np.asarray(
        Synth(
            type="dsss",
            fs=FS,
            seed=SEED_EMITTER,
            sps=SPC,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
            freq=CARRIER,
        ).steps(n),
        np.complex64,
    )
    for kind, a, b in spans():
        if kind == "off":
            sig[a * TE : b * TE] = 0.0
    amp = float(
        wfm_awgn_amplitude(ESN0_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    noise = AWGN(seed=seed, amplitude=amp).generate(n)
    return (sig + noise).astype(np.complex64)


# --8<-- [end:signal]

from doppler.dsss import AsyncDsssReceiver  # noqa: E402
from doppler.telemetry import MemoryCapture, Telemetry  # noqa: E402
from doppler.wfm import SampleClock  # noqa: E402

#: ``rx.state``'s values, in order: the receiver's ``ASYNC_DSSS_RX_*``
#: lifecycle states. The receiver names its current state as ``lifecycle``;
#: this tuple decodes the integers its telemetry carries, and
#: :func:`receive` checks it against ``lifecycle`` after every push.
STATES = ("searching", "refining", "tracking", "idle", "lost")
SEARCHING, REFINING, TRACKING, LOST = (
    STATES.index(s) for s in ("searching", "refining", "tracking", "lost")
)


# --8<-- [start:receiver]
def receive(x: np.ndarray) -> dict:
    """Feed the capture one code epoch per push, with the receiver's
    telemetry attached, and return everything it reported.

    ``doppler_uncertainty`` is one-sided: the search covers the carrier within
    ``CARRIER_UNCERTAINTY`` of nominal, in either direction. ``lost_confirm_s``
    arms the release rule. Everything else is the receiver's default.

    ``set_telemetry(tlm, "rx")`` registers every probe the receiver and its
    loops publish, under ``rx.``. The memory capture keeps every record, and
    ``set_now()`` stamps each push with its first sample, so every record
    carries the sample index of the push that wrote it.

    When the receiver reports ``lost``, the caller resets it. The receiver
    decides that an emitter has gone; the caller decides what to do about it.

    Returns
    -------
    series : dict
        Each probe's records by name, as ``(sample_index, value)`` arrays.
    """
    rx = AsyncDsssReceiver(
        code=GOLD,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=CN0_DBHZ,
        pfa=PFA,
        doppler_uncertainty=CARRIER_UNCERTAINTY,
        lost_confirm_s=LOST_CONFIRM_S,
    )
    tlm = Telemetry()
    rx.set_telemetry(tlm, "rx")
    emitted = 0
    with MemoryCapture(tlm, TE, SampleClock(FS)) as cap:
        for k in range(len(x) // TE):
            tlm.set_now(k * TE)
            emitted += len(rx.steps(x[k * TE : (k + 1) * TE]))
            assert STATES[rx.status().state] == rx.lifecycle
            if rx.lost:
                rx.reset()
    series = cap.read_dict(index=True)
    # The lock metric reports once an emitted symbol: its records count them.
    assert len(series["rx.sym.lock"][1]) == emitted
    return series


def per_push(series: dict, name: str, n: int) -> np.ndarray:
    """``name``'s value as each of ``n`` pushes left it.

    Each probe reports at its own rate: the state once a push, the symbol
    lock once a symbol, the code lock once a discriminator update. So the
    value a push left is the probe's last record at or before that push's
    stamp, carried over pushes that wrote none, and 0 before its first.
    """
    at, value = series[name]
    last = np.searchsorted(at, np.arange(n) * TE, side="right") - 1
    return np.where(last >= 0, value[np.maximum(last, 0)], 0.0)


def rows_of(series: dict) -> dict[str, np.ndarray]:
    """The receiver's records as one row per push: its state, its two lock
    decisions, its lock metric, the symbols it emitted and the Doppler it
    handed over."""
    n = len(series["rx.state"][1])  # one state record a push
    return {
        "state": per_push(series, "rx.state", n).astype(int),
        "code_locked": per_push(series, "rx.code.locked", n) > 0.5,
        "locked": per_push(series, "rx.sym.locked", n) > 0.5,
        "metric": per_push(series, "rx.sym.lock", n),
        # The metric reports once an emitted symbol, so its records count them.
        "symbols": np.bincount(
            (series["rx.sym.lock"][0] // TE).astype(np.int64), minlength=n
        ),
        "handover_hz": per_push(series, "rx.handover_hz", n),
    }


# --8<-- [end:receiver]


# --8<-- [start:validate]
def check(rows: dict[str, np.ndarray]) -> dict[str, float]:
    """Assert the lifecycle the receiver reported, and return the intervals
    and the hand-over it measured.

    The bounds are physical where a bound exists: the receiver cannot release
    an emitter before its confirm interval has passed, and it cannot emit a
    symbol before it tracks. The other bounds are generous windows around this
    run, chosen to hold the order of the events, not to pin a number.
    """
    ms = 1e3 * EPOCH_S
    seg = spans()
    on_at = seg[1][1]  # the first arrival
    fade = seg[2][1]  # the short fade
    depart = seg[4][1]  # the departure
    back = seg[5][1]  # the returning emitter
    state = rows["state"]
    assert len(state) == seg[-1][2], "the receiver did not report every push"

    # Noise alone: the receiver searches and emits nothing.
    assert (state[:on_at] == SEARCHING).all(), "left searching before arrival"

    # Arrival: tracking, and symbols only once tracking.
    tracking = state == TRACKING
    assert tracking.any(), "the receiver never reached tracking"
    first = int(np.argmax(tracking))
    acquire_ms = (first - on_at) * ms
    assert 5.0 < acquire_ms < 250.0, f"arrival to tracking {acquire_ms:.0f} ms"
    assert not rows["symbols"][:first].any(), "symbols before tracking"

    # This run's fade and the stretch to the departure: tracking holds, the
    # symbol lock stays up, and nothing is released before the emitter leaves.
    assert rows["locked"][fade - 1], (
        "this run: the lock was not up before the fade"
    )
    assert tracking[first:depart].all(), "left tracking before the departure"
    lost = state == LOST
    assert not lost[first:depart].any(), "LOST before the departure"
    assert rows["locked"][fade:depart].all(), (
        "this run: the lock fell before the departure"
    )

    # The departure: both lock detectors fall, and LOST comes only after the
    # confirm interval, since both must be down for that long first.
    assert lost[depart:back].any(), "the departure never released the emitter"
    released = depart + int(np.argmax(lost[depart:back]))
    assert (released - depart) * EPOCH_S >= LOST_CONFIRM_S, (
        "released before the confirm interval"
    )
    # Both flags are down for the whole confirm interval before the release.
    confirm = int(np.ceil(LOST_CONFIRM_S / EPOCH_S))
    assert not rows["code_locked"][released - confirm : released].any(), (
        "code lock was up during the confirm interval"
    )
    assert not rows["locked"][released - confirm : released].any(), (
        "symbol lock was up during the confirm interval"
    )
    release_ms = (released - depart) * ms

    # The reset: the caller's reset returns the receiver to searching.
    assert (state[released + 1 : back] == SEARCHING).all(), (
        "left searching during the gap after the reset"
    )

    # The return: the reset receiver acquires the new arrival afresh. It emits
    # no symbols until it tracks, and it tracks again within the bound.
    assert tracking[back:].any(), "the returning emitter was never acquired"
    reacquired = back + int(np.argmax(tracking[back:]))
    assert not rows["symbols"][released + 1 : reacquired].any(), (
        "symbols before the returning emitter was tracked"
    )
    return_ms = (reacquired - back) * ms
    assert 5.0 < return_ms < 250.0, (
        f"re-acquired {return_ms:.0f} ms after return"
    )
    symbols_since = int(rows["symbols"][reacquired:].sum())
    assert symbols_since > 0, "no symbols after re-acquiring"

    # The hand-over: the refine hands the tracker a Doppler inside the
    # caller's uncertainty, and the receiver reports it from then on.
    handover_hz = float(rows["handover_hz"][first])
    assert abs(handover_hz) <= CARRIER_UNCERTAINTY, (
        f"handed over at {handover_hz:.0f} Hz, outside the search"
    )

    lock_up_ms = int(np.argmax(rows["locked"][on_at:])) * ms
    return {
        "noise_ms": on_at * ms,
        "lock_up_ms": lock_up_ms,
        "acquire_ms": acquire_ms,
        "release_ms": release_ms,
        "reacquire_ms": return_ms,
        "symbols_since": symbols_since,
        "handover_hz": handover_hz,
        "fade_ms": (seg[2][2] - fade) * ms,
    }


# --8<-- [end:validate]


def main(out_path: str = "async_dsss_receiver_lifecycle.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    series = receive(make_timeline())
    rows = rows_of(series)
    t = check(rows)
    # The C twin prints these five lines from the same draw, and
    # test_c_example_pairs.py requires that the two faces print them alike.
    print(f"§1  noise alone for {t['noise_ms']:.0f} ms: searching, no symbols")
    print(
        f"§2  arrival to tracking: {t['acquire_ms']:.0f} ms, handed over at "
        f"{t['handover_hz']:.0f} Hz (the emitter at {CARRIER:.0f} Hz); "
        f"symbol lock up {t['lock_up_ms']:.0f} ms after arrival"
    )
    print(
        f"§3  a {t['fade_ms']:.0f} ms fade: tracking and the symbol lock held"
    )
    print(
        f"§4  departure: released {t['release_ms']:.0f} ms later, after the "
        f"{1e3 * LOST_CONFIRM_S:.0f} ms confirm interval"
    )
    print(
        f"§5  the return: re-acquired {t['reacquire_ms']:.0f} ms after it "
        f"began; {t['symbols_since']} symbols since"
    )

    t_ms = np.arange(len(rows["state"])) * EPOCH_S * 1e3
    # The carrier loop's own view of the carrier: its command, in cycles a
    # sample at the input rate, so times FS in Hz. The whole tracked carrier,
    # status().doppler_hz, also adds what the demodulator's carrier loop has
    # taken up beyond it.
    car_at, car = series["rx.car.nco"]
    # The loop reports only while it runs: break the line where it stopped.
    gap = np.flatnonzero(np.diff(car_at) > 2 * TE) + 1
    car_at = np.insert(car_at.astype(float), gap, np.nan)
    car = np.insert(car, gap, np.nan)
    ho_at, ho = series["rx.handover_hz"]
    ho = np.where(ho != 0.0, ho, np.nan)  # 0: no seed yet, nothing to draw

    fig, (top, mid, bot) = plt.subplots(3, 1, figsize=(10, 8), sharex=True)
    for ax in (top, mid, bot):
        for kind, a, b in spans():
            if kind == "off":
                ax.axvspan(
                    a * EPOCH_S * 1e3,
                    b * EPOCH_S * 1e3,
                    color="0.85",
                    lw=0,
                    label="signal off" if a == spans()[2][1] else None,
                )

    top.step(
        t_ms, rows["state"], where="post", color="k", lw=1.2, label="rx.state"
    )
    top.fill_between(
        t_ms,
        0,
        rows["locked"] * 4,
        step="post",
        color="#2ca02c",
        alpha=0.35,
        label="rx.sym.locked",
    )
    top.set_yticks(range(len(STATES)))
    top.set_yticklabels(STATES)
    top.set_ylim(-0.3, 4.6)
    top.set_title("State and symbol lock (grey: signal off)", fontsize=9)
    top.legend(fontsize=7, loc="upper left", ncol=3)
    top.grid(alpha=0.25)

    mid.plot(
        t_ms, rows["metric"], lw=1.0, color="#1f77b4", label="rx.sym.lock"
    )
    mid.set_title("Lock metric, the value the lock flag reads", fontsize=9)
    mid.set_ylabel("lock metric")
    mid.legend(fontsize=7, loc="lower right")
    mid.grid(alpha=0.25)

    bot.plot(
        car_at / FS * 1e3,
        car * FS,
        lw=0.8,
        color="#9467bd",
        label="rx.car.nco x fs",
    )
    bot.step(
        ho_at / FS * 1e3,
        ho,
        where="post",
        color="#ff7f0e",
        lw=1.2,
        label="rx.handover_hz",
    )
    bot.axhline(CARRIER, color="0.4", lw=0.8, ls=":", label="the emitter")
    bot.set_ylim(CARRIER - 400.0, CARRIER + 400.0)
    bot.set_title(
        "Carrier: the hand-over and the loop that pulls it in", fontsize=9
    )
    bot.set_xlabel("time (ms)")
    bot.set_ylabel("Hz")
    bot.legend(fontsize=7, loc="lower right", ncol=3)
    bot.grid(alpha=0.25)

    fig.suptitle(
        f"AsyncDsssReceiver: Gold-{SF}, {CHIP_RATE / 1e6:.3f} Mcps, "
        f"{SYM_RATE:.0f} bps, Es/N0={ESN0_DB:.0f} dB",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(
        sys.argv[1]
        if len(sys.argv) > 1
        else "async_dsss_receiver_lifecycle.png"
    )
