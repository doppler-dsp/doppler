"""async_dsss_receiver_lifecycle.py -- one
:class:`~doppler.dsss.AsyncDsssReceiver` through a signal's life.

The receiver follows one continuous DSSS emitter from the first code epoch it
searches to the moment the emitter leaves. It searches, refines the carrier
estimate, and tracks the emitter. When the emitter goes, both of its lock
detectors fall. Once they have been down for longer than a confirm
interval, the receiver moves to ``lost``. The caller resets it, and it
acquires the same emitter again when the emitter returns.

The receiver is fed one code epoch per push. Its state is read after every push
from its properties, and its symbols come from ``steps()``. Four things about
the states show in the run:

- **Searching and refining emit nothing.** The first symbols come once the
  receiver is tracking.
- **Tracking holds through a short fade.** The symbol lock stays up through it.
- **The release rule is armed.** ``lost_confirm_s`` is the interval both lock
  detectors must stay down before the receiver gives up on the emitter.
- **Reset returns the receiver to searching.** A receiver that has locked
  cannot be reset onto the same signal; it searches again, and a returning
  emitter is acquired afresh.

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


def make_timeline(seed: int = 2) -> np.ndarray:
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
    seed : int, default 2
        The noise draw. Each draw is one realisation of the same timeline; the
        lock-up time and whether a dropout holds depend on it.

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

# One row per push. The state is read from the receiver's own properties.
ROW = np.dtype(
    [
        ("searching", bool),
        ("refining", bool),
        ("tracking", bool),
        ("lost", bool),
        ("locked", bool),
        ("code_locked", bool),
        ("metric", float),
        ("symbols", int),
    ]
)


# --8<-- [start:receiver]
def receive(x: np.ndarray) -> np.ndarray:
    """Feed the capture one code epoch per push, and read the receiver after
    each one.

    ``doppler_uncertainty`` is one-sided: the search covers the carrier within
    ``CARRIER_UNCERTAINTY`` of nominal, in either direction. ``lost_confirm_s``
    arms the release rule. Everything else is the receiver's default.

    When the receiver reports ``lost``, the caller resets it. The receiver
    decides that an emitter has gone; the caller decides what to do about it.

    Returns one :data:`ROW` per push.
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
    rows = np.zeros(len(x) // TE, ROW)
    for k in range(len(rows)):
        block = rx.steps(x[k * TE : (k + 1) * TE])
        tracking, refining, lost = rx.tracking, rx.refining, rx.lost
        rows[k] = (
            not (tracking or refining or lost),
            refining,
            tracking,
            lost,
            rx.locked,
            rx.code_locked,
            rx.lock_metric,
            len(block),
        )
        if lost:
            rx.reset()
    return rows


# --8<-- [end:receiver]


# --8<-- [start:validate]
def check(rows: np.ndarray) -> dict[str, float]:
    """Assert the lifecycle the receiver went through, and return the
    intervals it measured, in milliseconds.

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

    # Noise alone: the receiver searches and emits nothing.
    assert rows["searching"][:on_at].all(), "left searching before the arrival"

    # Arrival: tracking, and symbols only once tracking.
    tracking = rows["tracking"]
    assert tracking.any(), "the receiver never reached tracking"
    first = int(np.argmax(tracking))
    acquire_ms = (first - on_at) * ms
    assert 5.0 < acquire_ms < 250.0, f"arrival to tracking {acquire_ms:.0f} ms"
    assert not rows["symbols"][:first].any(), "symbols before tracking"

    # The fade and the stretch to the departure: tracking holds, the symbol
    # lock stays up, and nothing is released before the emitter leaves.
    assert rows["locked"][fade - 1], "the lock was not up before the fade"
    assert tracking[first:depart].all(), "left tracking before the departure"
    assert not rows["lost"][first:depart].any(), "LOST before the departure"
    assert rows["locked"][fade:depart].all(), (
        "the lock fell before the departure"
    )

    # The departure: both lock detectors fall, and LOST comes only after the
    # confirm interval, since both must be down for that long first.
    lost = rows["lost"]
    assert lost[depart:back].any(), "the departure never released the emitter"
    released = depart + int(np.argmax(lost[depart:back]))
    assert (released - depart) * EPOCH_S >= LOST_CONFIRM_S, (
        "released before the confirm interval"
    )
    assert not rows["code_locked"][depart:released].all(), "code lock held"
    assert not rows["locked"][depart:released].all(), "symbol lock held"
    release_ms = (released - depart) * ms

    # The reset: the caller's reset returns the receiver to searching.
    assert rows["searching"][released + 1], "reset did not return to searching"

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
    assert rows["symbols"][reacquired:].sum() > 0, (
        "no symbols after re-acquiring"
    )

    return {
        "acquire_ms": acquire_ms,
        "release_ms": release_ms,
        "reacquire_ms": return_ms,
    }


# --8<-- [end:validate]


def main(out_path: str = "async_dsss_receiver_lifecycle.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rows = receive(make_timeline())
    times = check(rows)
    print(
        f"arrival to tracking {times['acquire_ms']:.0f} ms; the fade held the "
        f"lock; released {times['release_ms']:.0f} ms after departure, with a "
        f"{1e3 * LOST_CONFIRM_S:.0f} ms confirm interval; re-acquired "
        f"{times['reacquire_ms']:.0f} ms after the return"
    )

    t_ms = np.arange(len(rows)) * EPOCH_S * 1e3
    # Plot level for each state, from the row's flags.
    level = np.select(
        [rows["lost"], rows["tracking"], rows["refining"]], [3, 2, 1], 0
    )

    fig, (top, bot) = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
    for ax in (top, bot):
        for kind, a, b in spans():
            if kind == "off":
                ax.axvspan(
                    a * EPOCH_S * 1e3,
                    b * EPOCH_S * 1e3,
                    color="0.85",
                    lw=0,
                    label="signal off" if a == spans()[2][1] else None,
                )

    top.step(t_ms, level, where="post", color="k", lw=1.2, label="state")
    top.fill_between(
        t_ms,
        0,
        rows["locked"] * 4,
        step="post",
        color="#2ca02c",
        alpha=0.35,
        label="symbol lock",
    )
    top.set_yticks([0, 1, 2, 3])
    top.set_yticklabels(["searching", "refining", "tracking", "lost"])
    top.set_ylim(-0.3, 4.6)
    top.set_title("State and symbol lock (grey: signal off)", fontsize=9)
    top.legend(fontsize=7, loc="upper left", ncol=3)
    top.grid(alpha=0.25)

    bot.plot(
        t_ms, rows["metric"], lw=1.0, color="#1f77b4", label="lock_metric"
    )
    bot.set_title("Lock metric, the value the lock flag reads", fontsize=9)
    bot.set_xlabel("time (ms)")
    bot.set_ylabel("lock metric")
    bot.legend(fontsize=7, loc="lower right")
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
