"""async_dsss_receiver_lifecycle.py -- one
:class:`~doppler.dsss.AsyncDsssReceiver` through the life of a signal: noise
alone, arrival, acquisition, tracking, a short dropout, a long dropout, and a
departure.

The receiver is fed one code epoch per push, and after every push its own
:meth:`~doppler.dsss.AsyncDsssReceiver.status` is read, so every claim below
is a reading of the receiver, not of the test signal. Four things a caller
needs to know about the receiver's states fall out of the run:

- **Searching comes first and emits nothing.** While the receiver searches,
  and while it refines the carrier estimate, ``steps()`` returns no symbols.
  Symbols begin only once it is tracking.
- **Tracking is entered and held.** Once the carrier loop is live the state
  stays ``TRACKING`` for as long as the receiver is fed, through dropouts and
  through a departure, unless the release rule is armed.
- **Symbol lock is a health flag, separate from presence.** ``locked`` is a
  hysteretic detector on ``lock_metric``. In the fixed-seed run below a brief
  loss of signal does not clear it, a longer one does, and it returns only
  after the signal does. Whether a brief loss holds the lock is a probability
  over noise draws, and this example does not claim it.
  Its lock-up time trails ``TRACKING`` by an amount that depends on the
  noise draw, so this example does not claim one; it waits for the lock to
  be up before the first dropout and measures only what happens after.
- **The release rule is off by default.** ``lost_confirm_s`` defaults to
  ``0``, which never enters ``LOST``. A caller who wants the receiver to give
  up on a departed emitter sets it to a positive interval; this example
  leaves it at the default so the departure is visible as a lock change alone.

The design, and the release rule's reasoning, is in
``docs/design/async-dsss-receiver.md``.

Run:  python -m doppler.examples.async_dsss_receiver_lifecycle  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

SF = 1023  # CCSDS Gold code period, chips
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # 6.138 MSa/s
SYM_RATE = 2700.0  # asynchronous BPSK symbols per second
TE = SF * SPC  # one code epoch, 2,046 samples: one push
EPOCH_S = TE / FS  # 333.3 us of signal per push
CARRIER = 1500.0  # Hz, the emitter's carrier offset while it is on the air
DOPPLER_UNCERTAINTY = FS / TE  # 1/T_E, 3,000 Hz: one epoch resolves this span
ESN0_DB = 10.0  # Es/N0 of the emitter at the receiver input
CN0_DBHZ = ESN0_DB + 10.0 * np.log10(SYM_RATE)  # the receiver's C/N0 input
PFA = 1e-3

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: The timeline, in code epochs: ``"on"`` carries the signal, ``"off"`` is
#: noise alone. Written once, read by the signal, the figure and the checks.
TIMELINE = (
    ("off", 180),  # 60 ms of noise: nothing to find yet
    ("on", 4500),  # 1.5 s: acquire, refine, track, and let the lock come up
    ("off", 30),  # 10 ms dropout: short enough that the lock held in this run
    ("on", 300),  # 100 ms back on the air
    ("off", 150),  # 50 ms dropout: long enough for the lock to fall
    ("on", 180),  # 60 ms back on the air: long enough to re-lock
    ("off", 240),  # the emitter leaves for good
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

    :class:`~doppler.wfm.Synth` in continuous DSSS mode, with no code-only
    window: the Gold code repeats forever and the payload ``bits`` ride on it
    at the symbol rate, one bit a symbol. ``freq`` puts the carrier at
    :data:`CARRIER`.
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
    """A DSSS emitter that switches on and off over :data:`TIMELINE`, with
    noise running throughout.

    The clean signal is :func:`emitter`, sending :func:`payload_bits`. The cut
    is then made to the signal alone, after synthesis, so the noise floor does
    not change during a dropout. The noise is the library's own unit-noise
    ``Synth`` scaled by :func:`~doppler.wfm.wfm_awgn_amplitude`, which owns the
    Es/N0-to-sigma relation for the signal's sample rate.

    Returns
    -------
    x : NDArray[np.complex64]
        The whole capture, ``sum(n for _, n in TIMELINE)`` code epochs long.
    bits : NDArray[np.uint8]
        The payload the emitter sent, one bit a symbol, for the precondition.
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

# The state codes ``status().state`` reports, as the C ASYNC_DSSS_RX_* values.
SEARCHING, REFINING, TRACKING, LOST = 0, 1, 2, 4


# --8<-- [start:receiver]
def receive(x: np.ndarray):
    """Feed the capture one code epoch per push and read the receiver after
    each one.

    ``doppler_uncertainty`` is one epoch's resolution
    (:data:`DOPPLER_UNCERTAINTY`), which is what a single-epoch search must
    cover. The receiver is built with its defaults for everything else,
    including the release rule.

    Returns one row per push, ``(state, locked, lock_metric, symbols)``:
    the status read after the push, and how many symbols that push emitted.
    """
    rx = AsyncDsssReceiver(
        code=GOLD,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=CN0_DBHZ,
        pfa=PFA,
        doppler_uncertainty=DOPPLER_UNCERTAINTY,
    )
    rows = []
    for k in range(len(x) // TE):
        block = rx.steps(x[k * TE : (k + 1) * TE])
        st = rx.status()
        rows.append((st.state, st.locked, st.lock_metric, len(block)))
    return np.array(rows, float), rx


# --8<-- [end:receiver]


# --8<-- [start:validate]
def check(rows: np.ndarray, rx: AsyncDsssReceiver) -> dict[str, float]:
    """Assert the lifecycle the receiver went through, and return the
    intervals it measured, in milliseconds.

    The bounds are generous windows around one measured run, chosen to hold
    the order and the held-or-fell behavior, not to pin a number. They are not
    a measured spread across noise draws; that measurement is separate work.
    """
    state, locked, _metric, symbols = (rows[:, i] for i in range(4))
    ms = 1e3 * EPOCH_S
    seg = spans()
    on_at = seg[1][1]  # the epoch the signal arrives
    d1, d1_end = seg[2][1], seg[2][2]  # 10 ms dropout
    d2, d2_end = seg[4][1], seg[4][2]  # 50 ms dropout
    back_on_end = seg[5][2]  # the signal's return, up to the departure
    gone = seg[6][1]  # the departure

    tracking = state == TRACKING
    assert tracking.any(), "the receiver never reached tracking"
    first = int(np.argmax(tracking))
    acquire_ms = (first - on_at) * ms
    assert not tracking[:on_at].any(), "tracking before the signal arrived"
    assert 5.0 < acquire_ms < 250.0, f"arrival to tracking {acquire_ms:.0f} ms"
    assert not symbols[:first].any(), "symbols emitted before tracking"
    assert tracking[first:].all(), "left tracking without a release rule"
    assert not (state == LOST).any(), "entered LOST with lost_confirm_s = 0"

    # 10 ms dropout: the lock is up when the signal cuts, and it stays up
    # through the gap and for HOLD_WATCH_S (60 ms) after the return. That is
    # the definition of a held dropout in doppler#2408's validator
    # (async_dsss_receiver_lock.c). The lock metric dips just after the return
    # in this run, but the flag, which is what this example asserts on, does
    # not drop. Whether the hold holds across draws is a probability measured
    # in #2408, not asserted here.
    hold_end = min(d1_end + round(0.060 / EPOCH_S), d2)
    assert locked[d1 - 1] == 1, "the lock was not latched before the dropout"
    assert locked[d1:hold_end].all(), "the lock fell in the held window"

    # 50 ms dropout: the lock falls inside it, then returns after the signal.
    assert (locked[d2:d2_end] == 0).any(), "the lock never fell in 50 ms"
    fell = d2 + int(np.argmin(locked[d2:d2_end]))
    fall_ms = (fell - d2) * ms
    assert 5.0 < fall_ms < 45.0, f"lock fell {fall_ms:.0f} ms into the dropout"
    assert (locked[d2_end:back_on_end] == 1).any(), "the lock never returned"
    back = d2_end + int(np.argmax(locked[d2_end:back_on_end]))
    return_ms = (back - d2_end) * ms
    assert return_ms > 0.0, "the lock re-latched on noise during the gap"

    # The emitter gone for good: tracking holds, the lock falls once.
    assert (locked[gone:] == 0).any(), "the lock never fell after departure"
    gone_fall = gone + int(np.argmin(locked[gone:]))
    gone_ms = (gone_fall - gone) * ms
    assert 5.0 < gone_ms < 60.0, f"lock fell {gone_ms:.0f} ms after departure"
    assert tracking[gone:].all(), (
        "departure moved the receiver out of tracking"
    )

    return {
        "acquire_ms": acquire_ms,
        "fall_in_50_ms": fall_ms,
        "return_ms": return_ms,
        "fall_after_departure_ms": gone_ms,
    }


def check_data_flips(bits: np.ndarray, window: int = 20) -> float:
    """Assert the stimulus precondition: every event in the timeline is cut
    while the data is transitioning, and return the flip fraction.

    This is a precondition of the test, not a claim about the symbol lock.
    The test is meant to run on a data-carrying signal, so the payload must
    change from one symbol to the next around each dropout and the departure.
    It is checked on the bits the emitter sent, so no despreading is needed:

    - the fraction of symbol boundaries where the bit changes is about one
      half, which is what random data gives;
    - within ``window`` symbols of each cut the bit changes at least once.

    A cut at epoch ``e`` falls at symbol ``e * TE / (FS / SYM_RATE)``, which
    maps the timeline onto the payload.
    """
    flips = bits[1:] != bits[:-1]
    frac = float(flips.mean())
    assert 0.35 < frac < 0.65, f"data flips at {frac:.2f} of boundaries"
    sym_len = FS / SYM_RATE
    seg = spans()
    for edge in (seg[2][1], seg[2][2], seg[4][1], seg[4][2], seg[6][1]):
        k = int(edge * TE / sym_len)
        lo, hi = max(k - window, 0), min(k + window, len(flips))
        assert flips[lo:hi].any(), (
            f"no data flip within {window} symbols of epoch {edge}"
        )
    return frac
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
    rows, rx = receive(x)
    times = check(rows, rx)
    print(
        f"arrival to tracking {times['acquire_ms']:.0f} ms; lock held through "
        f"the 10 ms dropout and its 60 ms watch; fell "
        f"{times['fall_in_50_ms']:.0f} ms into 50 ms and returned "
        f"{times['return_ms']:.0f} ms after the signal; fell "
        f"{times['fall_after_departure_ms']:.0f} ms after departure"
    )

    state, locked, metric = rows[:, 0], rows[:, 1], rows[:, 2]
    t_ms = np.arange(len(rows)) * EPOCH_S * 1e3
    tracking = (state == TRACKING).astype(float)

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

    top.step(t_ms, tracking, where="post", color="k", lw=1.2, label="tracking")
    top.fill_between(
        t_ms,
        0,
        locked,
        step="post",
        color="#2ca02c",
        alpha=0.45,
        label="symbol lock",
    )
    top.set_yticks([0, 1])
    top.set_yticklabels(["no", "yes"])
    top.set_ylim(-0.1, 1.2)
    top.set_title("State and symbol lock (grey: signal off)", fontsize=9)
    top.legend(fontsize=7, loc="upper left", ncol=3)
    top.grid(alpha=0.25)

    bot.plot(t_ms, metric, lw=1.0, color="#1f77b4", label="lock_metric")
    bot.set_title(
        "Lock metric, the value the flag reads: how far it dipped", fontsize=9
    )
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
