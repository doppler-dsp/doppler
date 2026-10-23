"""async_dsss_pool_scene.py -- ten emitters come and go through one
:class:`~doppler.dsss.AsyncDsssPool`, each on its own moving Doppler.

Six receivers share one stream. Ten emitters on one Gold code reach it over
six seconds, at most six on the air at once, each at its own code phase and
level (within +-8 dB of one another) and each on its own Doppler: a start
and a rate, as from its own motion, applied to the chip clock as well as
the carrier (:class:`~doppler.impairment.DopplerChannel`). Every receiver
searches the whole carrier range for itself; the pool's controller only
keeps them apart, and frees a receiver when its emitter leaves.

What the run shows:

- **Each emitter is held by one receiver, from soon after it arrives to
  when it leaves.** The receiver's carrier follows the emitter's Doppler
  ramp the whole time, and its symbol lock is up.
- **A departure frees a receiver, and a newcomer takes it.** Ten emitters
  pass through six receivers.
- **A weak emitter waits for the strong one that masks it.** A search that
  skips the signals already held cannot tell a second signal far below a
  stronger one from that one's sidelobes. On a one-epoch search a signal
  10 dB below a stronger one is taken first in about two streams in three,
  and 13 dB below in about one in three (design ``async-dsss-parallel.md``
  §3). F, 13 dB below C, waits here and is taken once C leaves.

The pool is fed one code epoch per push. The checks and the figure read
only what the pool reports: each receiver's ``status()`` after every push.
An emitter is identified by its carrier and code phase, which this file
knows because it placed each emitter on the stream.

Run:  python -m doppler.examples.async_dsss_pool_scene  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.impairment import DopplerChannel
from doppler.source import AWGN
from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

SF = 1023  # Gold code period, chips (CCSDS)
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # sample rate, 6.138 MSa/s
SYM_RATE = 2700.0  # BPSK symbols per second, asynchronous to the code
TE = SF * SPC  # one code epoch, samples: one push
EPOCH_S = TE / FS  # seconds per push
RF_HZ = 2.2e9  # the carrier the Doppler is referred to: 1 ppm = 2200 Hz
CARRIER_UNCERTAINTY = 5000.0  # Hz, one-sided: the search range
ESN0_REF_DB = 20.0  # Es/N0 of an emitter at level 0 dB
LOST_CONFIRM_S = 0.1  # both lock detectors down this long before a release
N_RECEIVERS = 6
SEED_NOISE = 5

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: The emitters: name, level (dB, against ESN0_REF_DB), Doppler at arrival
#: (ppm) and its rate (ppm/s), code-phase offset (samples into its first
#: epoch) and the epochs it is on the air, [first, end).
EMITTERS = (
    ("A", +0.0, +1.20, -0.10, 0, (90, 12000)),
    ("B", -6.0, -1.50, +0.15, 600, (600, 6000)),
    ("C", +5.0, +0.30, +0.05, 1300, (1200, 9000)),
    ("D", -3.0, -0.40, -0.12, 1800, (2100, 18000)),
    ("E", +8.0, +1.80, -0.20, 300, (3000, 7500)),
    ("F", -8.0, -1.00, +0.08, 900, (6600, 15000)),
    ("G", +2.0, +0.80, +0.10, 1500, (7800, 18000)),
    ("H", -5.0, -1.90, +0.18, 200, (9600, 16500)),
    ("I", +6.0, +0.00, -0.15, 1100, (12300, 18000)),
    ("J", -2.0, -0.70, -0.06, 1700, (13500, 18000)),
)
N_EPOCHS = 18000  # 6 s


def channel(k: int) -> DopplerChannel:
    """Emitter ``k``'s Doppler: a clock offset and its rate, on RF_HZ."""
    _, _, ppm, rate, _, _ = EMITTERS[k]
    return DopplerChannel(
        fs=FS, carrier_hz=RF_HZ, doppler_ppm=ppm, doppler_rate_ppm_s=rate
    )


def make_stream(seed: int = SEED_NOISE) -> np.ndarray:
    """The capture: :data:`EMITTERS` on one Gold code, each from
    :class:`~doppler.wfm.Synth` in continuous DSSS mode with its own PRBS
    seed, through its own :func:`channel`, scaled to its level and placed
    on the stream at its own sample offset, plus
    :class:`~doppler.source.AWGN` at the noise a level-0 emitter sees as
    ESN0_REF_DB.

    Returns
    -------
    x : NDArray[np.complex64]
        :data:`N_EPOCHS` code epochs.
    """
    n = N_EPOCHS * TE
    x = np.zeros(n, np.complex64)
    for k, (_, level, _, _, offset, (a, b)) in enumerate(EMITTERS):
        s0, s1 = a * TE + offset, b * TE
        # A clock running fast consumes more input than it emits: render a
        # little extra so the channel never runs dry.
        clean = Synth(
            type="dsss",
            fs=FS,
            seed=11 + k,  # each emitter its own data
            sps=SPC,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
        ).steps(int((s1 - s0) * 1.001) + 64)
        y = np.asarray(channel(k).execute(clean), np.complex64)[: s1 - s0]
        x[s0:s1] += y * np.float32(10.0 ** (level / 20.0))
    amp = float(
        wfm_awgn_amplitude(ESN0_REF_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    x += AWGN(seed=seed, amplitude=amp).generate(n).astype(np.complex64)
    return x


def truth(k: int, epochs: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Emitter ``k``'s carrier (Hz) and code phase (chips) at the end of
    each of ``epochs``. The Doppler is linear in time; the code phase is
    the nominal one plus the chips the stretched clock has gained, less the
    channel's group delay (:attr:`DopplerChannel.delay_samples`)."""
    _, _, ppm, rate, offset, (a, _) = EMITTERS[k]
    t = (np.asarray(epochs) + 1 - a) * EPOCH_S
    f = RF_HZ * 1e-6 * (ppm + rate * t)
    gained = CHIP_RATE * 1e-6 * (ppm * t + rate * t * t / 2.0)
    late = channel(k).delay_samples / SPC
    nominal = (t * FS - offset) / SPC
    return f, (nominal + gained - late) % SF


# --8<-- [end:signal]

from doppler.dsss import AsyncDsssPool  # noqa: E402


# --8<-- [start:pool]
def run(x: np.ndarray) -> dict:
    """Push the capture one code epoch at a time and record every
    receiver's status after each push.

    The pool is built like one :class:`~doppler.dsss.AsyncDsssReceiver`,
    plus ``n_receivers``. ``carrier_freq_hz`` tells each receiver the RF
    carrier, so its code loop follows the clock stretch its carrier implies;
    ``cn0_dbhz`` is the design point, the weakest emitter's C/N0; and
    ``lost_confirm_s`` arms the release rule that frees a receiver whose
    emitter left.

    Returns
    -------
    rec : dict
        ``state``, ``doppler``, ``phase``, ``locked`` and ``code_locked``,
        each ``(epochs, receivers)``, and the pool's release counts.
    """
    weakest = min(e[1] for e in EMITTERS)
    pool = AsyncDsssPool(
        GOLD,
        n_receivers=N_RECEIVERS,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=ESN0_REF_DB + weakest + 10.0 * np.log10(SYM_RATE),
        doppler_uncertainty=CARRIER_UNCERTAINTY,
        lost_confirm_s=LOST_CONFIRM_S,
        carrier_freq_hz=RF_HZ,
    )
    shape = (N_EPOCHS, N_RECEIVERS)
    rec = {
        "state": np.zeros(shape, int),
        "doppler": np.zeros(shape),
        "phase": np.zeros(shape),
        "locked": np.zeros(shape, int),
        "code_locked": np.zeros(shape, int),
    }
    for e in range(N_EPOCHS):
        pool.push(x[e * TE : (e + 1) * TE])
        for i in range(N_RECEIVERS):
            st = pool.status(i)
            pool.symbols(i)  # drained every push, as a consumer would
            rec["state"][e, i] = st.state
            rec["doppler"][e, i] = st.doppler_hz
            rec["phase"][e, i] = st.chip_phase
            rec["locked"][e, i] = st.locked
            rec["code_locked"][e, i] = st.code_locked
    rec["released"] = {
        "lost": pool.released_lost,
        "duplicate": pool.released_duplicate,
        "ghost": pool.released_ghost,
    }
    return rec


# --8<-- [end:pool]


# --8<-- [start:validate]
# The level gap past which a one-epoch search is not expected to take the
# weaker signal: docs/design/async-dsss-parallel.md §3 takes it first in
# 65% of streams at -10 dB, 36% at -13 dB and 1% at -16 dB.
MASK_DB = 10.0
SETTLE_S = 0.5  # time allowed from arrival (or unmasking) to a hold
GONE_S = LOST_CONFIRM_S + 0.2  # time allowed from departure to release


def holder_of(rec: dict) -> np.ndarray:
    """Which emitter each receiver holds at each epoch's end: the emitter
    whose carrier is within 60 Hz and code phase within 3 chips of the
    receiver's, or -1. A receiver still holding an emitter that has left
    reads that emitter: it is matched against the emitter's extrapolated
    truth for the rest of the stream, so one that is never released shows."""
    out = np.full(rec["state"].shape, -1)
    ep = np.arange(N_EPOCHS)
    tracking = rec["state"] == 2
    for k, (_, _, _, _, _, (a, _b)) in enumerate(EMITTERS):
        span = ep >= a
        f, ph = truth(k, ep)
        for i in range(N_RECEIVERS):
            d = np.abs(rec["phase"][:, i] - ph)
            near = (np.abs(rec["doppler"][:, i] - f) < 60.0) & (
                np.minimum(d, SF - d) < 3.0
            )
            out[span & tracking[:, i] & near, i] = k
    return out


def unmasked_from(k: int) -> int:
    """The first epoch emitter ``k`` is on the air with no other emitter on
    the air more than MASK_DB above it: from then on a search can tell it
    from the others' sidelobes. A stronger one arriving after it is held
    does not matter; it is the search that has the limit."""
    _, level, _, _, _, (a, b) = EMITTERS[k]
    for e in range(a, b):
        if not any(
            other - level > MASK_DB and oa <= e < ob
            for _, other, _, _, _, (oa, ob) in EMITTERS
        ):
            return e
    return b


def check(rec: dict) -> dict:
    """The run's claims, asserted from the pool's own reports."""
    held = holder_of(rec)
    settle = int(SETTLE_S / EPOCH_S)
    gone = int(GONE_S / EPOCH_S)
    taken_ms = {}
    for k, (name, _, _, _, _, (a, b)) in enumerate(EMITTERS):
        u = unmasked_from(k)
        n_holders = (held == k).sum(axis=1)
        # Never two receivers on one emitter.
        assert n_holders.max() <= 1, f"{name}: held twice"
        # Not held while a stronger emitter masks it (F, until C leaves) ...
        assert not n_holders[a:u].any(), f"{name}: held while masked"
        # ... then held from SETTLE_S after it can be told from the others
        assert n_holders[u:b].any(), f"{name}: never taken"
        first = u + int(np.flatnonzero(n_holders[u:b])[0])
        assert first - u <= settle, f"{name}: taken late"
        taken_ms[name] = (first - a) * EPOCH_S * 1e3
        # to its departure, but for the odd epoch a loop slips.
        assert n_holders[first:b].mean() > 0.98, f"{name}: dropped"
        # Symbol-locked while it is held and on the air, once the lock has
        # had SETTLE_S to come up.
        on = np.zeros(N_EPOCHS, bool)
        on[first + settle : b] = True
        locked = rec["locked"][(held == k) & on[:, None]]
        assert locked.mean() > 0.99, f"{name}: lock flickers"
        # Released after it leaves.
        if b < N_EPOCHS:
            assert not n_holders[b + gone :].any(), f"{name}: held after"
    # Ten emitters passed through six receivers.
    assert len(EMITTERS) > N_RECEIVERS
    assert rec["released"]["lost"] >= len(EMITTERS) - N_RECEIVERS
    # F is the masked one: C is the emitter whose departure unmasks it,
    # which the hold-while-masked check above then holds the run to.
    f_k = [e[0] for e in EMITTERS].index("F")
    c_end = EMITTERS[[e[0] for e in EMITTERS].index("C")][5][1]
    assert unmasked_from(f_k) == c_end > EMITTERS[f_k][5][0]
    return {"taken_ms": taken_ms, "held": held}


# --8<-- [end:validate]

COLORS = [
    "#1f77b4",
    "#ff7f0e",
    "#2ca02c",
    "#d62728",
    "#9467bd",
    "#8c564b",
    "#e377c2",
    "#7f7f7f",
    "#bcbd22",
    "#17becf",
]


def main(out_path: str = "async_dsss_pool_scene.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rec = run(make_stream())
    res = check(rec)
    held = res["held"]
    for name, ms in res["taken_ms"].items():
        print(f"{name}: held {ms:.0f} ms after it arrived")
    r = rec["released"]
    print(
        f"releases: {r['lost']} lost, {r['duplicate']} duplicate, "
        f"{r['ghost']} ghost"
    )

    ep = np.arange(N_EPOCHS)
    t_s = ep * EPOCH_S
    fig, (top, bot) = plt.subplots(
        2,
        1,
        figsize=(10, 8.5),
        sharex=True,
        gridspec_kw={"height_ratios": [2.2, 1]},
    )
    # Doppler against time: each emitter's truth while on the air, faint
    # and wide, and each receiver's tracked carrier over it where it holds
    # that emitter.
    for k, (name, level, _, _, _, (a, b)) in enumerate(EMITTERS):
        on = ep[a:b]
        f, _ = truth(k, on)
        top.plot(on * EPOCH_S, f, color=COLORS[k], lw=7, alpha=0.22)
        top.text(
            a * EPOCH_S,
            f[0] + 120,
            f"{name} {level:+.0f} dB",
            color=COLORS[k],
            fontsize=8,
            weight="bold",
        )
    for i in range(N_RECEIVERS):
        for k in range(len(EMITTERS)):
            f = np.where(held[:, i] == k, rec["doppler"][:, i], np.nan)
            top.plot(t_s, f, color=COLORS[k], lw=1.4)
    # F's wait: on the air, but more than MASK_DB below C until C leaves.
    names = [e[0] for e in EMITTERS]
    f_k = names.index("F")
    u = unmasked_from(f_k)
    f_u, _ = truth(f_k, np.array([u]))
    top.annotate(
        f"F waits: C is {EMITTERS[names.index('C')][1] - EMITTERS[f_k][1]:.0f}"
        " dB stronger until it leaves",
        xy=(u * EPOCH_S, f_u[0]),
        xytext=(u * EPOCH_S - 0.9, f_u[0] - 1100),
        fontsize=8,
        color=COLORS[f_k],
        arrowprops={"arrowstyle": "->", "color": COLORS[f_k], "lw": 0.8},
    )
    top.set_ylim(-CARRIER_UNCERTAINTY, CARRIER_UNCERTAINTY)
    top.set_ylabel("Doppler (Hz)")
    top.set_title(
        "Each emitter's Doppler on the air (wide, faint) and the carrier "
        "of the receiver that holds it (thin)",
        fontsize=9,
    )
    top.grid(alpha=0.25)
    # Symbol lock per receiver, coloured by the emitter it holds.
    for i in range(N_RECEIVERS):
        y = N_RECEIVERS - 1 - i
        for k in range(len(EMITTERS)):
            on = (held[:, i] == k) & (rec["locked"][:, i] == 1)
            edges = np.flatnonzero(np.diff(np.r_[0, on, 0]))
            bot.broken_barh(
                [
                    (s * EPOCH_S, (f - s) * EPOCH_S)
                    for s, f in zip(edges[::2], edges[1::2])
                ],
                (y + 0.15, 0.7),
                color=COLORS[k],
            )
        busy = (rec["state"][:, i] != 0) & (rec["locked"][:, i] == 0)
        bot.fill_between(
            t_s,
            y + 0.15,
            y + 0.85,
            where=busy,
            step="post",
            color="0.75",
            lw=0,
            label="held, not symbol-locked" if i == 0 else None,
        )
    bot.set_yticks([y + 0.5 for y in range(N_RECEIVERS)])
    bot.set_yticklabels(
        [f"receiver {N_RECEIVERS - 1 - y}" for y in range(N_RECEIVERS)],
        fontsize=8,
    )
    bot.set_xlim(0, N_EPOCHS * EPOCH_S)
    bot.set_xlabel("time (s)")
    bot.set_title(
        "Symbol lock, status().locked (colour = emitter held; gaps = "
        "searching)",
        fontsize=9,
    )
    bot.legend(fontsize=7, loc="lower left")
    bot.grid(alpha=0.25, axis="x")
    fig.suptitle(
        f"AsyncDsssPool: {len(EMITTERS)} emitters through {N_RECEIVERS} "
        f"receivers on Gold-{SF}, Es/N0 {ESN0_REF_DB:.0f} dB +- 8 dB",
        fontsize=11,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=110)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(*sys.argv[1:2])
