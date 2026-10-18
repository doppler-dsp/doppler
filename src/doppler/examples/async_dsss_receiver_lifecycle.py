"""async_dsss_receiver_lifecycle.py -- one
:class:`~doppler.dsss.AsyncDsssReceiver` through a signal's life.

The receiver follows one continuous DSSS emitter from the first code epoch it
searches to the moment the emitter leaves. It searches, refines the carrier
estimate, tracks the emitter, rides out a short fade, and releases the
emitter once both of its lock detectors have been down for longer than a
confirm interval. The caller then resets it, and it acquires the same emitter
again when it returns.

The receiver is fed one code epoch per push, and its state is read through
:meth:`~doppler.dsss.AsyncDsssReceiver.status` after every push. Symbols come
from :meth:`~doppler.dsss.AsyncDsssReceiver.steps`. Four things about the
states fall out of the run:

- **Searching emits nothing.** The receiver emits no symbols while it searches
  or refines. The first symbols come once it is tracking.
- **Tracking holds through a fade.** A fade shorter than the lock detectors'
  memory leaves the state ``TRACKING`` and the symbol lock up.
- **The release rule is armed here.** Both lock detectors down, continuously,
  for longer than ``lost_confirm_s`` moves the receiver to ``LOST``. Its loops
  stop and it discards samples until ``reset()``.
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

from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

# ── Setup: this scenario's numbers ─────────────────────────────────────────
# The caller knows the sample rate, the chip and symbol rates, and the
# carrier uncertainty it has. The receiver takes those, and the C/N0 it
# needs to size its detectors.
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
LOST_CONFIRM_S = 0.1  # both lock flags down this long before LOST

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: The timeline, in code epochs: ``"on"`` carries the signal, ``"off"`` is
#: noise alone. Written once, read by the signal, the receiver run, the
#: checks and the figure.
TIMELINE = (
    ("off", 180),  # 60 ms of noise: nothing to find yet
    ("on", 4500),  # 1.5 s: acquire, refine, track, and let the lock come up
    ("off", 30),  # 10 ms fade: shorter than the lock detectors' memory
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


def payload_bits(n: int, seed: int = 7) -> np.ndarray:
    """The data the emitter sends over ``n`` samples: one bit per symbol.

    The stimulus defines its own bits, so the data is known exactly and the
    precondition is checked on the bits themselves. The count covers every
    symbol the capture spans, with a few spare at the end.
    """
    n_sym = int(n / (FS / SYM_RATE)) + 8
    return np.random.default_rng(seed).integers(0, 2, n_sym).astype(np.uint8)


def emitter(n: int, bits: np.ndarray) -> np.ndarray:
    """The clean emitter, ``n`` samples, sending ``bits`` throughout.

    :class:`~doppler.wfm.Synth` in continuous DSSS mode: the Gold code repeats
    and the payload ``bits`` ride on it at the symbol rate, one bit a symbol.
    ``freq`` puts the carrier at :data:`CARRIER`.
    """
    return np.asarray(
        Synth(
            type="dsss",
            fs=FS,
            sps=SPC,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
            freq=CARRIER,
            data=bits,
        ).steps(n),
        np.complex64,
    )


def make_timeline(seed: int = 7) -> tuple[np.ndarray, np.ndarray]:
    """The capture: an emitter switching on and off over :data:`TIMELINE`,
    with noise running throughout.

    The clean signal is :func:`emitter`. Each ``"off"`` span is zeroed after
    synthesis, so the noise floor is the same before, during and after every
    cut. The noise is the library's own unit-noise ``Synth`` scaled by
    :func:`~doppler.wfm.wfm_awgn_amplitude`, which owns the Es/N0-to-sigma
    relation for the signal's sample rate.

    Returns
    -------
    x : NDArray[np.complex64]
        The whole capture, ``sum(n for _, n in TIMELINE)`` code epochs long.
    bits : NDArray[np.uint8]
        The payload the emitter sent, one bit a symbol.
    """
    n = sum(n for _, n in TIMELINE) * TE
    bits = payload_bits(n, seed)
    sig = emitter(n, bits)
    for kind, a, b in spans():
        if kind == "off":
            sig[a * TE : b * TE] = 0.0
    # wfm_awgn_amplitude returns a per-component sigma; the Synth emits unit
    # total power (0.707 per component), so sqrt(2) bridges the two.
    amp = float(
        wfm_awgn_amplitude(ESN0_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    unit = np.asarray(Synth(type="noise", fs=FS, seed=seed + 1).steps(n))
    x = (sig + amp * np.sqrt(2.0) * unit).astype(np.complex64)
    return x, bits


# --8<-- [end:signal]

from doppler.dsss import AsyncDsssReceiver  # noqa: E402

# The state codes ``status().state`` reports.
SEARCHING, REFINING, TRACKING, LOST = 0, 1, 2, 4


# --8<-- [start:receiver]
def receive(x: np.ndarray):
    """Feed the capture one code epoch per push, and read the receiver after
    each one.

    ``doppler_uncertainty`` is one-sided: the search covers the carrier within
    ``CARRIER_UNCERTAINTY`` of nominal, in either direction. ``lost_confirm_s``
    arms the release rule. Everything else is the receiver's default.

    When the receiver reaches ``LOST``, the caller resets it. The receiver
    decides that an emitter has gone; the caller decides what to do about it.

    Returns one row per push, ``(state, locked, lock_metric, symbols)``: the
    status read after the push, and how many symbols that push emitted.
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
    rows = []
    for k in range(len(x) // TE):
        block = rx.steps(x[k * TE : (k + 1) * TE])
        st = rx.status()
        rows.append((st.state, st.locked, st.lock_metric, len(block)))
        if st.state == LOST:
            rx.reset()
    return np.array(rows, float)


# --8<-- [end:receiver]


# --8<-- [start:validate]
def check(rows: np.ndarray) -> dict[str, float]:
    """Assert the lifecycle the receiver went through, and return the
    intervals it measured, in milliseconds.

    The bounds are physical where a bound exists: the receiver cannot release
    an emitter before its confirm interval has passed, and it cannot emit a
    symbol before it tracks. The rest are generous windows around the run,
    chosen to hold the order of the events, not to pin a number.
    """
    state, locked, _metric, symbols = (rows[:, i] for i in range(4))
    ms = 1e3 * EPOCH_S
    seg = spans()
    on_at = seg[1][1]  # the first arrival
    fade, fade_end = seg[2][1], seg[2][2]  # the short fade
    depart = seg[4][1]  # the first departure
    back = seg[5][1]  # the returning emitter

    # Arrival: searching first, then tracking, then symbols.
    tracking = state == TRACKING
    assert not tracking[:on_at].any(), "tracking before the signal arrived"
    assert tracking.any(), "the receiver never reached tracking"
    first = int(np.argmax(tracking))
    acquire_ms = (first - on_at) * ms
    assert 5.0 < acquire_ms < 250.0, f"arrival to tracking {acquire_ms:.0f} ms"
    assert not symbols[:first].any(), "symbols emitted before tracking"

    # The fade: tracking and the symbol lock hold through it.
    assert locked[fade - 1] == 1, "the lock was not up before the fade"
    assert tracking[fade:fade_end].all(), "the fade left tracking"
    assert locked[fade:fade_end].all(), "the fade cleared the symbol lock"

    # The departure: the lock falls, and LOST comes only after the confirm
    # interval, since both flags must be down for longer than that first.
    lost = state == LOST
    assert lost[depart:back].any(), "the emitter's departure never released it"
    released = depart + int(np.argmax(lost[depart:back]))
    assert (released - depart) * EPOCH_S >= LOST_CONFIRM_S, (
        "released before the confirm interval"
    )
    release_ms = (released - depart) * ms
    assert locked[depart:released].min() == 0, "the lock never fell"
    assert state[released + 1] == SEARCHING, "reset did not return to search"

    # The return: the reset receiver acquires the new arrival afresh. It emits
    # no symbols until it tracks, and it tracks again within the bound.
    reacquired = back + int(np.argmax(tracking[back:]))
    assert tracking[back:].any(), "the returning emitter was never acquired"
    assert not symbols[released + 1 : reacquired].any(), (
        "symbols emitted before the returning emitter was tracked"
    )
    return_ms = (reacquired - back) * ms
    assert 5.0 < return_ms < 250.0, (
        f"re-acquired {return_ms:.0f} ms after return"
    )
    assert symbols[reacquired:].sum() > 0, "no symbols after re-acquiring"

    return {
        "acquire_ms": acquire_ms,
        "release_ms": release_ms,
        "reacquire_ms": return_ms,
    }


def check_data_flips(bits: np.ndarray, window: int = 20) -> float:
    """Assert the stimulus precondition: every event in the timeline is cut
    while the data is transitioning, and return the flip fraction.

    This is a precondition of the test, not a claim about the symbol lock.
    The payload must change from one symbol to the next around each cut, so
    no event falls in a code-only run. It is checked on the bits the emitter
    sent:

    - the fraction of symbol boundaries where the bit changes is about one
      half, which is what random data gives;
    - within ``window`` symbols of each cut the bit changes at least once.

    A cut at epoch ``e`` falls at symbol ``e * TE / (FS / SYM_RATE)``.
    """
    flips = bits[1:] != bits[:-1]
    frac = float(flips.mean())
    assert 0.35 < frac < 0.65, f"data flips at {frac:.2f} of boundaries"
    sym_len = FS / SYM_RATE
    seg = spans()
    for edge in (seg[2][1], seg[2][2], seg[4][1], seg[5][1]):
        k = int(edge * TE / sym_len)
        lo, hi = max(k - window, 0), min(k + window, len(flips))
        assert flips[lo:hi].any(), (
            f"no data flip within {window} symbols of epoch {edge}"
        )
    return frac


# --8<-- [end:validate]


def main(out_path: str = "async_dsss_receiver_lifecycle.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x, bits = make_timeline()
    flip_fraction = check_data_flips(bits)
    print(
        f"stimulus precondition: data flips at {flip_fraction:.2f} of symbol "
        "boundaries, and within 20 symbols of every cut"
    )
    rows = receive(x)
    times = check(rows)
    print(
        f"arrival to tracking {times['acquire_ms']:.0f} ms; the fade held the "
        f"lock; released {times['release_ms']:.0f} ms after departure with a "
        f"{1e3 * LOST_CONFIRM_S:.0f} ms confirm interval; re-acquired "
        f"{times['reacquire_ms']:.0f} ms after the return"
    )

    state, locked, metric = rows[:, 0], rows[:, 1], rows[:, 2]
    t_ms = np.arange(len(rows)) * EPOCH_S * 1e3

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

    top.step(t_ms, state, where="post", color="k", lw=1.2, label="state")
    top.fill_between(
        t_ms,
        0,
        locked * 4,
        step="post",
        color="#2ca02c",
        alpha=0.35,
        label="symbol lock",
    )
    top.set_yticks([SEARCHING, TRACKING, LOST])
    top.set_yticklabels(["searching", "tracking", "lost"])
    top.set_ylim(-0.3, 4.6)
    top.set_title("State and symbol lock (grey: signal off)", fontsize=9)
    top.legend(fontsize=7, loc="upper left", ncol=3)
    top.grid(alpha=0.25)

    bot.plot(t_ms, metric, lw=1.0, color="#1f77b4", label="lock_metric")
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
