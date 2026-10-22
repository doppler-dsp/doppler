"""async_dsss_pool_demo.py -- an :class:`~doppler.dsss.AsyncDsssPool` holding
several emitters that come and go.

Three receivers share one stream. Four emitters on one Gold code reach it at
different times, code phases and carriers: A, B and C arrive one after
another, B leaves, and D arrives after it. Every receiver searches the
whole carrier range for itself; the pool's controller only keeps them apart.
It gives each searching receiver the code phases the others hold, so a
search never takes a signal already held, and a rank, so two receivers that
decide in the same dwell take different signals. When a receiver's emitter
leaves, its release rule fires, the controller resets it, and it searches
again.

What the run shows:

- **One receiver per emitter.** While A, B and C are on the air, each is
  held by exactly one receiver, and no two receivers hold one signal.
- **A departure frees a receiver.** After B leaves, its receiver is
  released once both of its lock detectors have stayed down for
  ``lost_confirm_s``, and it searches again.
- **The freed receiver takes the newcomer.** D is acquired by the receiver
  that was free, without disturbing the two that track A and C.

The pool is fed one code epoch per push. The checks and the figure read
only what the pool reports: each receiver's ``status()``, after every push.
An emitter is identified by its code phase, which this file knows because it
placed each emitter on the stream.

The design is ``docs/design/async-dsss-parallel.md``.

Run:  python -m doppler.examples.async_dsss_pool_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.source import AWGN
from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

SF = 1023  # Gold code period, chips (CCSDS)
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # sample rate, 6.138 MSa/s
SYM_RATE = 2700.0  # BPSK symbols per second, asynchronous to the code
TE = SF * SPC  # one code epoch, samples: one push
EPOCH_S = TE / FS  # seconds per push
CARRIER_UNCERTAINTY = 5000.0  # Hz, one-sided: the search range
ESN0_DB = 10.0  # Es/N0 of each emitter at the input
CN0_DBHZ = ESN0_DB + 10.0 * np.log10(SYM_RATE)  # C/N0, dB-Hz
LOST_CONFIRM_S = 0.1  # both lock detectors down this long before a release
N_RECEIVERS = 3
SEED_NOISE = 5

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: The emitters: name, carrier (Hz), code-phase offset (samples into its
#: first epoch, so each sits hundreds of chips from the others), and the
#: epochs it is on the air, [first, end).
EMITTERS = (
    ("A", 1500.0, 0, (90, 9600)),
    ("B", -2200.0, 600, (900, 4500)),
    ("C", 700.0, 1300, (1800, 9600)),
    ("D", -1000.0, 1800, (5700, 9600)),
)
N_EPOCHS = 9600  # 3.2 s


def make_stream(seed: int = SEED_NOISE) -> np.ndarray:
    """The capture: :data:`EMITTERS` on one Gold code, each from
    :class:`~doppler.wfm.Synth` in continuous DSSS mode with its own PRBS
    seed, placed on the stream at its own sample offset and carrier, plus
    :class:`~doppler.source.AWGN` at the Es/N0 every emitter shares.

    Returns
    -------
    x : NDArray[np.complex64]
        :data:`N_EPOCHS` code epochs.
    """
    n = N_EPOCHS * TE
    x = np.zeros(n, np.complex64)
    for k, (_, freq, offset, (a, b)) in enumerate(EMITTERS):
        s0, s1 = a * TE + offset, b * TE
        x[s0:s1] += np.asarray(
            Synth(
                type="dsss",
                fs=FS,
                seed=11 + k,  # each emitter its own data
                sps=SPC,
                data_code=GOLD.tobytes(),
                symbol_rate=SYM_RATE,
                freq=freq,
            ).steps(s1 - s0),
            np.complex64,
        )
    amp = float(
        wfm_awgn_amplitude(ESN0_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    x += AWGN(seed=seed, amplitude=amp).generate(n).astype(np.complex64)
    return x


def emitter_phase(k: int, sample: int) -> float:
    """Emitter ``k``'s code phase, chips, at ``sample``: its code starts at
    chip 0 on its first sample."""
    _, _, offset, (a, _) = EMITTERS[k]
    return ((sample - (a * TE + offset)) / SPC) % SF


# --8<-- [end:signal]

from doppler.dsss import AsyncDsssPool  # noqa: E402

STATES = {0: "searching", 1: "refining", 2: "tracking", 4: "lost"}


# --8<-- [start:pool]
def run(x: np.ndarray) -> dict:
    """Push the capture one code epoch at a time and record every
    receiver's status after each push.

    The pool is built like one :class:`~doppler.dsss.AsyncDsssReceiver`,
    plus ``n_receivers``. ``lost_confirm_s`` arms each receiver's release
    rule, which is what frees a receiver whose emitter left.

    Returns
    -------
    rec : dict
        ``state``, ``phase`` and ``locked``, each ``(epochs, receivers)``;
        ``symbols``, each receiver's last 600 symbols; and the pool's
        ``released_lost`` and ``released_duplicate`` counts.
    """
    pool = AsyncDsssPool(
        GOLD,
        n_receivers=N_RECEIVERS,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=CN0_DBHZ,
        doppler_uncertainty=CARRIER_UNCERTAINTY,
        lost_confirm_s=LOST_CONFIRM_S,
    )
    shape = (N_EPOCHS, N_RECEIVERS)
    state = np.zeros(shape, int)
    phase = np.zeros(shape)
    locked = np.zeros(shape, int)
    tail = [[] for _ in range(N_RECEIVERS)]
    rx1_t, rx1_s = [], []  # every symbol receiver 1 emits, and when
    for e in range(N_EPOCHS):
        pool.push(x[e * TE : (e + 1) * TE])
        for i in range(N_RECEIVERS):
            st = pool.status(i)
            state[e, i], phase[e, i], locked[e, i] = (
                st.state,
                st.chip_phase,
                st.locked,
            )
            sym = pool.symbols(i)
            tail[i] = (tail[i] + list(sym))[-600:]
            if i == 1 and len(sym):
                # Spread over the push that emitted them: a symbol's time
                # to within one code epoch.
                rx1_t.extend(
                    e * EPOCH_S + EPOCH_S * np.arange(len(sym)) / len(sym)
                )
                rx1_s.extend(sym)
    return {
        "state": state,
        "phase": phase,
        "locked": locked,
        "symbols": [np.asarray(t) for t in tail],
        "rx1_t": np.asarray(rx1_t),
        "rx1_s": np.asarray(rx1_s),
        "released_lost": pool.released_lost,
        "released_duplicate": pool.released_duplicate,
    }


# --8<-- [end:pool]


# --8<-- [start:validate]
def holder_of(rec: dict) -> np.ndarray:
    """Which emitter each receiver holds at each epoch's end: the index of
    the emitter within 1.5 chips of the receiver's code phase, or -1
    (searching). A receiver still holding an emitter that has left -- until
    its release fires -- reads that emitter."""
    out = np.full(rec["state"].shape, -1)
    for e in range(N_EPOCHS):
        end = (e + 1) * TE
        for i in range(N_RECEIVERS):
            if rec["state"][e, i] not in (1, 2):
                continue
            for k, (_, _, _, (a, _b)) in enumerate(EMITTERS):
                d = abs(rec["phase"][e, i] - emitter_phase(k, end))
                if e >= a and min(d, SF - d) <= 1.5:
                    out[e, i] = k
    return out


def check(rec: dict) -> dict:
    """The run's claims, asserted from the pool's own reports.

    Settling margins: an emitter is counted from 0.5 s after it arrives,
    and a departed one from ``lost_confirm_s`` plus 0.2 s after it leaves.
    """
    held = holder_of(rec)
    settle = int(0.5 / EPOCH_S)
    gone = int((LOST_CONFIRM_S + 0.2) / EPOCH_S)
    arrive_ms = {}
    for k, (name, _, _, (a, b)) in enumerate(EMITTERS):
        span = held[a + settle : b]
        n_holders = (span == k).sum(axis=1)
        assert (n_holders == 1).all(), (
            f"{name}: held by {set(n_holders)} receivers while on the air"
        )
        first = np.flatnonzero((held[a:b] == k).any(axis=1))[0]
        arrive_ms[name] = first * EPOCH_S * 1e3
        if b < N_EPOCHS:  # left: released within the confirm time
            assert not (held[b + gone :] == k).any(), (
                f"{name} still held after it left"
            )
            last = np.flatnonzero((held == k).any(axis=1))[-1]
            release_ms = (last + 1 - b) * EPOCH_S * 1e3
    # A departure freed a receiver, and the newcomer took it.
    assert rec["released_lost"] >= 1
    b_rx = int(np.flatnonzero((held == 1).any(axis=0))[0])
    d_rx = int(np.flatnonzero((held == 3).any(axis=0))[0])
    assert b_rx == d_rx, "D was taken by the receiver B freed"
    # Every receiver holding a signal at the end has symbol lock.
    for i in range(N_RECEIVERS):
        assert rec["locked"][-1, i] == 1, f"receiver {i} not symbol-locked"
    return {"arrive_ms": arrive_ms, "b_rx": b_rx, "release_ms": release_ms}


# --8<-- [end:validate]


def main(out_path: str = "async_dsss_pool_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rec = run(make_stream())
    t = check(rec)
    held = holder_of(rec)
    for name, ms in t["arrive_ms"].items():
        print(f"{name}: held {ms:.0f} ms after it arrived")
    print(
        f"B left; receiver {t['b_rx']} was released "
        f"{t['release_ms']:.0f} ms later "
        f"({rec['released_lost']} release(s), "
        f"{rec['released_duplicate']} duplicate(s)) and took D"
    )

    colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728"]
    t_s = np.arange(N_EPOCHS) * EPOCH_S
    fig = plt.figure(figsize=(10, 9.5))
    gs = fig.add_gridspec(3, N_RECEIVERS, height_ratios=[1.3, 0.9, 1])
    top = fig.add_subplot(gs[0, :])
    # The emitters on the air, then what each receiver holds.
    for k, (name, freq, _, (a, b)) in enumerate(EMITTERS):
        top.broken_barh(
            [(a * EPOCH_S, (b - a) * EPOCH_S)],
            (N_RECEIVERS + 0.6 + 0.5 * k, 0.4),
            color=colors[k],
            alpha=0.5,
        )
        top.text(
            a * EPOCH_S + 0.02,
            N_RECEIVERS + 0.8 + 0.5 * k,
            f"{name} ({freq:+.0f} Hz)",
            fontsize=7,
            va="center",
        )
    for i in range(N_RECEIVERS):
        y = N_RECEIVERS - 1 - i
        for k in range(len(EMITTERS)):
            on = held[:, i] == k
            edges = np.flatnonzero(np.diff(np.r_[0, on, 0]))
            top.broken_barh(
                [
                    (s * EPOCH_S, (f - s) * EPOCH_S)
                    for s, f in zip(edges[::2], edges[1::2])
                ],
                (y + 0.1, 0.8),
                color=colors[k],
            )
        for k, (_, _, _, (_, end)) in enumerate(EMITTERS):
            late = (held[:, i] == k) & (np.arange(N_EPOCHS) >= end)
            if late.any():
                s0, s1 = np.flatnonzero(late)[[0, -1]]
                top.broken_barh(
                    [(s0 * EPOCH_S, (s1 + 1 - s0) * EPOCH_S)],
                    (y + 0.1, 0.8),
                    facecolor="none",
                    edgecolor="k",
                    hatch="///",
                    lw=0,
                    label="held after it left" if i == 1 else None,
                )
        busy = rec["state"][:, i] == 1
        top.fill_between(
            t_s,
            y + 0.1,
            y + 0.9,
            where=busy,
            step="post",
            color="0.6",
            lw=0,
            label="refining" if i == 0 else None,
        )
    top.set_yticks(
        [y + 0.5 for y in range(N_RECEIVERS)]
        + [N_RECEIVERS + 0.8 + 0.5 * k for k in range(len(EMITTERS))]
    )
    top.set_yticklabels(
        [f"receiver {N_RECEIVERS - 1 - i}" for i in range(N_RECEIVERS)]
        + ["on air"] * len(EMITTERS),
        fontsize=8,
    )
    top.set_xlim(0, N_EPOCHS * EPOCH_S)
    top.set_xlabel("time (s)")
    top.set_title(
        "What each receiver holds (colour = emitter; gaps = searching)",
        fontsize=9,
    )
    top.legend(fontsize=7, loc="lower left")
    top.grid(alpha=0.25, axis="x")
    # Receiver 1 over time: B's symbols, silence while it searches after
    # the release, then D's.
    mid = fig.add_subplot(gs[1, :], sharex=top)
    mid.plot(
        rec["rx1_t"], rec["rx1_s"].real, ".", ms=1.2, color="k", label="I"
    )
    mid.plot(
        rec["rx1_t"], rec["rx1_s"].imag, ".", ms=1.2, color="0.6", label="Q"
    )
    for k in (1, 3):
        on = held[:, 1] == k
        idx = np.flatnonzero(on)
        if idx.size:
            mid.axvspan(
                idx[0] * EPOCH_S,
                (idx[-1] + 1) * EPOCH_S,
                color=colors[k],
                alpha=0.12,
                lw=0,
            )
    mid.set_ylim(-2.0, 2.0)
    mid.set_ylabel("symbol")
    # The symbol lock flag, status().locked, on its own 0/1 axis: what says
    # the symbols are a signal's (it drops when B leaves, before the
    # release, and the receiver emits noise until then).
    lk = mid.twinx()
    lk.step(
        t_s,
        rec["locked"][:, 1],
        where="post",
        color="#9467bd",
        lw=1.4,
        label="symbol lock",
    )
    lk.set_ylim(-0.1, 1.1)
    lk.set_yticks([0, 1])
    lk.set_ylabel("locked", color="#9467bd")
    # One legend, on the left, holding both axes' entries; drawn on the
    # twin, which sits on top.
    h1, l1 = mid.get_legend_handles_labels()
    h2, l2 = lk.get_legend_handles_labels()
    lk.legend(h1 + h2, l1 + l2, fontsize=7, loc="upper left", markerscale=6)
    mid.set_xlabel("time (s)")
    mid.set_title(
        "Receiver 1's symbols and its symbol lock: B, then "
        "searching, then D (shading = emitter held)",
        fontsize=9,
    )
    mid.grid(alpha=0.25)
    for i in range(N_RECEIVERS):
        ax = fig.add_subplot(gs[2, i])
        s = rec["symbols"][i]
        k = held[-1, i]
        ax.plot(s.real, s.imag, ".", ms=2, color=colors[k] if k >= 0 else "k")
        ax.set_aspect("equal")
        lim = 1.6 * max(1e-9, np.abs(s).max())
        ax.set_xlim(-lim, lim)
        ax.set_ylim(-lim, lim)
        ax.set_title(
            f"receiver {i}: {EMITTERS[k][0] if k >= 0 else '-'}, "
            "last 600 symbols",
            fontsize=8,
        )
        ax.grid(alpha=0.25)
    fig.suptitle(
        f"AsyncDsssPool: {N_RECEIVERS} receivers, 4 emitters on Gold-{SF}, "
        f"Es/N0={ESN0_DB:.0f} dB each",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_dsss_pool_demo.png")
