"""async_dsss_pool_ghost.py -- the duplicate an
:class:`~doppler.dsss.AsyncDsssPool` cannot see yet: a receiver holding a
strong emitter's Doppler-offset sidelobe.

A strong emitter A and a weaker one B share a Gold code. A search lists
every peak above its gate, and a strong signal's list holds more than the
signal: its code, correlated a Doppler tile away from its carrier, also
peaks at other code phases, hundreds of chips from its own. A receiver that
takes one of those peaks -- a *ghost* -- refines it, tracks it and holds
symbol lock, because what it despreads is A's own data through a partial
correlation.

The pool's controller keys duplicates on code phase: two receivers within a
chip hold one signal. A ghost sits hundreds of chips from A, so to the
controller it is a second signal, and it occupies a receiver for as long as
A is on the air. The pool's searchers already drop most sidelobes (a peak
A's own sidelobes could explain is not taken), but one passes at the
search's own false-alarm rate. This run puts one there on purpose and shows
what tells it apart:

- **Code phase does not.** The ghost tracks 200 chips or more from A, far
  outside the one-chip zone, and B, a real second signal, is just as far.
- **Its code lock does, weakly.** A partial correlation keeps the Dll's own
  detector low while the symbol lock rides on A's data. But a weak true
  emitter's code lock flickers too (doppler#2529), so it cannot carry a
  release.
- **Its symbols do.** Differential symbols, correlated over 50-symbol
  windows at the best lag within +-60, read well above any distinct pair
  for the ghost against A, and low for B against A. That is the rule the
  owner agreed on doppler#2529: of two receivers whose data correlate, the
  weaker releases. It is not in the pool yet; this file computes the
  statistic itself to show what the rule will see.

The three receivers here are standalone
:class:`~doppler.dsss.AsyncDsssReceiver`\\ s, each ``seed()``\\ ed from one
dwell of a probe :class:`~doppler.acquire.Acquisition` built like their own
search and listing 16 peaks. Seeding is how a pool's receiver would be
handed the same peak; doing it by hand makes the ghost happen every run.

Run:  python -m doppler.examples.async_dsss_pool_ghost  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.acquire import Acquisition
from doppler.dsss import AsyncDsssReceiver
from doppler.source import AWGN
from doppler.wfm import Gold, Synth

SF = 1023  # Gold code period, chips (CCSDS)
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # sample rate, 6.138 MSa/s
SYM_RATE = 2700.0  # BPSK symbols per second, asynchronous to the code
TE = SF * SPC  # one code epoch, samples
EPOCH_S = TE / FS
CARRIER_UNCERTAINTY = 5000.0  # Hz, one-sided: the search range
DESIGN_CN0 = 47.0  # dB-Hz the receivers are sized for
DURATION_S = 2.0
WIN, N_WIN, MAX_LAG = 50, 20, 60  # the statistic (doppler#2529)
THRESHOLD = 0.25  # the threshold #2529's measurement recommends

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: name, carrier (Hz), C/N0 (dB-Hz), code-phase offset (samples), data seed.
#: A sits halfway between two Doppler tiles (3 kHz apart), where its
#: Doppler-offset sidelobes are strongest.
EMITTERS = (
    ("A", 1500.0, 70.0, 0, 11),
    ("B", -2200.0, 55.0, 700, 12),
)


def make_stream(seed: int = 3) -> np.ndarray:
    """A and B, each from :class:`~doppler.wfm.Synth` in continuous DSSS
    mode with its own random data bits (one per symbol), plus
    :class:`~doppler.source.AWGN` setting A's C/N0.

    Each emitter's data is its own draw: the synth's built-in data source
    gives every emitter one sequence, shifted, which would make two
    distinct emitters correlate (doppler#2529).
    """
    n = int(DURATION_S * FS) // TE * TE
    x = np.zeros(n, np.complex64)
    for _, freq, cn0, offset, dseed in EMITTERS:
        m = n - offset
        bits = np.random.default_rng(dseed).integers(
            0, 2, int(m / FS * SYM_RATE) + 16
        )
        sig = Synth(
            type="dsss",
            fs=FS,
            sps=SPC,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
            freq=freq,
            data=bits.astype(np.uint8),
        ).steps(m)
        # Unit power is 70 dB-Hz against the noise below.
        x[offset:] += np.asarray(sig, np.complex64) * np.float32(
            10 ** ((cn0 - 70.0) / 20)
        )
    sigma = 1.0 / np.sqrt(10 ** (70.0 / 10) / FS)
    x += AWGN(seed=seed, amplitude=float(sigma / np.sqrt(2))).generate(n)
    return x.astype(np.complex64)


def true_phase(k: int, sample: int) -> float:
    """Emitter ``k``'s code phase, chips, at ``sample``."""
    return ((sample - EMITTERS[k][3]) / SPC) % SF


def chip_dist(a: float, b: float) -> float:
    d = abs(a - b) % SF
    return min(d, SF - d)


# --8<-- [end:signal]


# --8<-- [start:probe]
def probe(x: np.ndarray) -> dict:
    """The first dwell whose peak list holds A, B and a third peak at least
    50 chips from both: that peak is a sidelobe (whose, the data tells).

    The probe is an :class:`~doppler.acquire.Acquisition` with the
    receivers' own search parameters, listing up to 16 peaks a dwell.
    Each listed peak's record carries the hand-off chip phase (field 7),
    the Doppler bin (field 0) and the C/N0 estimate (field 5), at the
    sample the dwell ended (field 6): what ``seed()`` takes.

    Returns
    -------
    seeds : dict
        ``{"A" | "B" | "ghost": (chip_phase, doppler_hz, cn0_dbhz)}`` and
        ``"at"``, the sample to feed the receivers from.
    """
    acq = Acquisition(
        GOLD,
        spc=SPC,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        cn0_dbhz=DESIGN_CN0,
        doppler_uncertainty=CARRIER_UNCERTAINTY,
    )
    acq.set_max_peaks(16)
    hz = np.zeros(acq.surface_rows)
    acq.surface_doppler_hz(hz)
    for e in range(len(x) // TE):
        found = {}
        for r in acq.push(x[e * TE : (e + 1) * TE]):
            at, phase = int(r[6]), float(r[7])
            seed = (phase, float(hz[r[0]]), float(r[5]))
            d = [chip_dist(phase, true_phase(k, at)) for k in range(2)]
            if min(d) <= 1.5:
                name = EMITTERS[int(np.argmin(d))][0]
            elif min(d) >= 50.0:
                name = "ghost"
            else:
                continue  # near a signal but not on it: neither
            found.setdefault(name, seed)  # strongest of each first
        if len(found) == 3:
            return {**found, "at": at}
    raise AssertionError("no dwell listed A, B and a sidelobe together")


# --8<-- [end:probe]


# --8<-- [start:receivers]
def run(x: np.ndarray, seeds: dict) -> dict:
    """Three receivers, each ``seed()``\\ ed from the probe's dwell and fed
    from its end, one code epoch per call. ``reacquire`` and a 0.4 s
    ``lost_confirm_s`` arm the release: a receiver on noise lets go."""
    out = {}
    for name in ("A", "B", "ghost"):
        rx = AsyncDsssReceiver(
            GOLD,
            chip_rate=CHIP_RATE,
            symbol_rate=SYM_RATE,
            spc=SPC,
            cn0_dbhz=DESIGN_CN0,
            doppler_uncertainty=CARRIER_UNCERTAINTY,
            lost_confirm_s=0.4,
            reacquire=True,
        )
        rx.seed(*seeds[name])
        syms, phase, locked, code_locked, t = [], [], [], [], []
        for p in range(seeds["at"], len(x) - TE + 1, TE):
            syms.extend(rx.steps(x[p : p + TE]))
            st = rx.status()
            t.append((p + TE) / FS)
            phase.append(st.chip_phase)
            locked.append(st.locked)
            code_locked.append(st.code_locked)
        out[name] = {
            "symbols": np.asarray(syms),
            "t": np.asarray(t),
            "phase": np.asarray(phase),
            "locked": np.asarray(locked),
            "code_locked": np.asarray(code_locked),
        }
    return out


# --8<-- [end:receivers]


# --8<-- [start:statistic]
def correlation(x: np.ndarray, y: np.ndarray) -> float:
    """doppler#2529's statistic: differential symbols ``d_k = s_k
    conj(s_{k-1})`` (no carrier phase to resolve), then per 50-symbol window
    ``|sum d_y conj(d_x)| / sqrt(sum|d_y|^2 sum|d_x|^2)``, averaged over up
    to 20 windows, at the best lag within +-60 symbols."""
    dx, dy = x[1:] * np.conj(x[:-1]), y[1:] * np.conj(y[:-1])
    best = 0.0
    for lag in range(-MAX_LAG, MAX_LAG + 1):
        a, b = dx[max(0, lag) :], dy[max(0, -lag) :]
        m = min(len(a), len(b), WIN * N_WIN) // WIN * WIN
        if m == 0:
            continue
        a, b = a[:m].reshape(-1, WIN), b[:m].reshape(-1, WIN)
        num = np.abs((b * np.conj(a)).sum(axis=1))
        den = np.sqrt(
            (np.abs(b) ** 2).sum(axis=1) * (np.abs(a) ** 2).sum(axis=1)
        )
        best = max(best, float((num / den).mean()))
    return best


def correlation_series(x, y, block: int = 1000):
    """The statistic over blocks of ``block`` symbols, half-overlapped:
    returns (block-centre symbol index, value) arrays."""
    n = min(len(x), len(y))
    starts = range(0, n - block + 1, block // 2)
    return (
        np.array([s + block // 2 for s in starts]),
        np.array(
            [correlation(x[s : s + block], y[s : s + block]) for s in starts]
        ),
    )


# --8<-- [end:statistic]


# --8<-- [start:validate]
def check(rec: dict) -> dict:
    """What the run shows, asserted from the receivers' own reports. The
    first 0.3 s after the seed is settling and is not judged."""
    settle = int(0.3 / EPOCH_S)
    g, a = rec["ghost"], rec["A"]
    sep = np.array(
        [
            chip_dist(p, q)
            for p, q in zip(g["phase"][settle:], a["phase"][settle:])
        ]
    )
    assert sep.min() > 100.0, "the ghost sits far outside the one-chip zone"
    assert g["locked"][settle:].mean() > 0.8, "the ghost holds symbol lock"
    assert g["code_locked"][settle:].mean() < 0.2, "its code lock stays low"
    for name in ("A", "B"):
        assert rec[name]["locked"][settle:].mean() > 0.8, f"{name} locked"
    _, rg = correlation_series(a["symbols"], g["symbols"])
    _, rb = correlation_series(a["symbols"], rec["B"]["symbols"])
    assert rg.min() > THRESHOLD, "every block: the ghost correlates with A"
    assert rb.max() < THRESHOLD, "every block: B does not"
    return {
        "sep": sep.min(),
        "ghost": (rg.min(), rg.max()),
        "B": (rb.min(), rb.max()),
        "code_locked": g["code_locked"][settle:].mean(),
    }


# --8<-- [end:validate]


def main(out_path: str = "async_dsss_pool_ghost.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    x = make_stream()
    seeds = probe(x)
    rec = run(x, seeds)
    c = check(rec)
    print(
        f"ghost seeded at chip {seeds['ghost'][0]:.0f}, "
        f"{seeds['ghost'][1]:+.0f} Hz; A at chip {seeds['A'][0]:.0f}"
    )
    print(
        f"ghost to A: at least {c['sep']:.0f} chips apart; code lock up "
        f"{100 * c['code_locked']:.0f}% of the time"
    )
    print(
        f"correlation with A: ghost {c['ghost'][0]:.2f}..{c['ghost'][1]:.2f},"
        f" B {c['B'][0]:.2f}..{c['B'][1]:.2f} (threshold {THRESHOLD})"
    )

    colors = {"A": "#1f77b4", "B": "#2ca02c", "ghost": "#d62728"}
    fig, (top, mid, bot) = plt.subplots(3, 1, figsize=(10, 9))
    a_phase = rec["A"]["phase"]
    for name in ("B", "ghost"):
        r = rec[name]
        n = min(len(r["phase"]), len(a_phase))
        d = [chip_dist(p, q) for p, q in zip(r["phase"][:n], a_phase[:n])]
        top.plot(
            r["t"][:n], d, lw=1.6, color=colors[name], label=f"{name} receiver"
        )
    top.axhspan(
        0,
        1.0,
        color="k",
        alpha=0.25,
        label="duplicate zone: within one chip of A",
    )
    top.set_yscale("symlog", linthresh=1.0)
    top.set_ylim(0, SF / 2)
    top.set_ylabel("chips from A's code phase")
    top.set_xlabel("time (s)")
    top.set_title(
        "Code phase: the ghost is as far from A as B is, so the "
        "pool's one-chip key reads two signals",
        fontsize=9,
    )
    top.legend(fontsize=7, loc="center left")
    top.grid(alpha=0.25)

    g = rec["ghost"]
    ts = np.linspace(g["t"][0], g["t"][-1], len(g["symbols"]))
    mid.plot(
        ts,
        g["symbols"].real,
        ".",
        ms=1.0,
        color=colors["ghost"],
        label="ghost receiver's symbols (I)",
    )
    mid.set_ylim(-2.2, 2.2)
    mid.set_ylabel("symbol")
    mid.set_xlabel("time (s)")
    lk = mid.twinx()
    lk.step(
        g["t"],
        g["locked"],
        where="post",
        color="#9467bd",
        lw=1.4,
        label="symbol lock",
    )
    lk.step(
        g["t"],
        g["code_locked"] * 0.5,
        where="post",
        color="k",
        lw=1.2,
        label="code lock (x0.5)",
    )
    lk.set_ylim(-0.1, 1.1)
    lk.set_yticks([0, 0.5, 1])
    h1, l1 = mid.get_legend_handles_labels()
    h2, l2 = lk.get_legend_handles_labels()
    lk.legend(h1 + h2, l1 + l2, fontsize=7, loc="upper left", markerscale=6)
    mid.set_title(
        "The ghost: symbol lock held on A's data, code lock down", fontsize=9
    )
    mid.grid(alpha=0.25)

    for name in ("ghost", "B"):
        idx, val = correlation_series(
            rec["A"]["symbols"], rec[name]["symbols"]
        )
        bot.plot(
            rec["A"]["t"][0] + idx / SYM_RATE,
            val,
            "o-",
            ms=4,
            color=colors[name],
            label=f"{name} vs A",
        )
    bot.axhline(
        THRESHOLD,
        color="k",
        ls="--",
        lw=1,
        label=f"threshold {THRESHOLD} (#2529)",
    )
    bot.axhline(0.8, color="0.5", ls=":", lw=1, label="0.8, first proposed")
    bot.set_ylim(0, 1)
    bot.set_ylabel("data correlation")
    bot.set_xlabel("time (s)")
    bot.set_title(
        "What the rule will read: the ghost's data correlates "
        "with A's, B's does not",
        fontsize=9,
    )
    bot.legend(fontsize=7, loc="upper left")
    bot.grid(alpha=0.25)

    fig.suptitle(
        f"A ghost: A at {EMITTERS[0][2]:.0f} dB-Hz, B at "
        f"{EMITTERS[1][2]:.0f} dB-Hz, receivers designed for "
        f"{DESIGN_CN0:.0f} dB-Hz, Gold-{SF}",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_dsss_pool_ghost.png")
