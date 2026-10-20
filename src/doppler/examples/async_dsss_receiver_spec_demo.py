"""async_dsss_receiver_spec_demo.py -- one
:class:`~doppler.dsss.AsyncDsssReceiver` on a link whose Doppler moves the
code clock as well as the carrier.

The receiver demodulates continuous DSSS whose data symbols are asynchronous
to the code epoch. On a moving platform the Doppler shifts the carrier and
stretches the chip clock by the same factor, v/c. Told the RF carrier
(``carrier_freq_hz``), the receiver feeds the carrier offset its carrier loop
tracks to the code loop as the matching code-rate offset, so the code loop
follows a clock its own discriminator could not pull in.

The receiver searches its carrier uncertainty, refines the estimate, and then
tracks and emits symbols. Its symbol lock, ``locked``, says when those symbols
are the signal's. With ``reacquire`` armed, a hand-off whose carrier never
comes up to symbol lock is released once ``lost_confirm_s`` has passed, and
the receiver searches again by itself. A missed hand-off costs a retry, not
the link.

A pass stresses the receiver in two ways that never coincide. Near closest
approach the Doppler ramps through zero at its fastest rate. At the ends of
the pass it sits at its extreme offset and barely moves. This script runs the
receiver through both regimes over a set of noise draws and checks the decode
three ways:

- **BER** against the known payload, with :class:`~doppler.ber.BerMeter`. The
  meter detects where the symbol stream sits against the payload by
  correlating a marker; it does not search for the alignment with the fewest
  errors. The pooled rate is quoted as an implementation loss: how many dB
  more Es/N0 coherent BPSK would need to make the same errors.
- **EVM**, :func:`~doppler.ber.ber_evm_db`: each symbol against its own hard
  decision. It normalizes by the measured total power, so a perfect
  receiver reads 10*log10(N/(S+N)), a little below -Es/N0.
- **M2M4**, :func:`~doppler.snr.snr_m2m4_db`: a blind SNR estimate from the
  symbol moments. It assumes circular noise, and the demodulated stream's
  noise is not circular, so it reads above the input Es/N0. It is a lock
  check here, not a measurement of the link.

The script reads the receiver through ``status()`` and ``lifecycle`` after
every push, and takes the symbols from ``steps()``. The design is in
``docs/design/async-dsss-receiver.md``.

Run:  python -m doppler.examples.async_dsss_receiver_spec_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.impairment import DopplerChannel
from doppler.source import AWGN
from doppler.wfm import PN, Gold, Synth, wfm_awgn_amplitude

# ── Setup: this scenario's numbers ─────────────────────────────────────────
# The link: a 1023-chip Gold code at 3.069 Mcps carrying BPSK at 2700 bps on a
# 2.5 GHz carrier, with up to +/-50 kHz of Doppler changing by up to 500 Hz/s.
SF = 1023  # Gold code period, chips (CCSDS)
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip, so the sample rate is CHIP_RATE * SPC
FS = CHIP_RATE * SPC  # sample rate, 6.138 MSa/s
SYM_RATE = 2700.0  # BPSK symbols per second, asynchronous to the code
TE = SF * SPC  # one code epoch, samples: one push
CARRIER_HZ = 2.5e9  # the RF carrier the Doppler is referred to
CARRIER_UNCERTAINTY_HZ = 50e3  # one-sided: the Doppler is within +/-50 kHz
DOPPLER_RATE_HZ_S = 500.0  # the fastest the Doppler changes
ESN0_DB = 8.0  # Es/N0 at the receiver input
CN0_DBHZ = ESN0_DB + 10.0 * np.log10(SYM_RATE)  # the same, as C/N0 in dB-Hz
# Symbol lock down this long: release, search again. Sized past the pull-in of
# a hand-off that does lock, which the script prints, so none is cut short.
LOST_CONFIRM_S = 0.4

N_SYM = 6000  # payload symbols, 2.2 s of signal
DURATION_S = N_SYM / SYM_RATE
LEAD_IN = 20 * TE + 737  # noise alone first; not a whole number of epochs
DRAWS = range(1, 9)  # one noise seed per draw

#: The two regimes of a pass, as (Doppler at the start, Hz; rate, Hz/s).
#: The TCA crossing starts below zero so the ramp crosses zero mid-capture.
REGIMES = {
    "TCA crossing": (-0.5 * DOPPLER_RATE_HZ_S * DURATION_S, DOPPLER_RATE_HZ_S),
    "+50 kHz extremum": (CARRIER_UNCERTAINTY_HZ, 0.0),
    "-50 kHz extremum": (-CARRIER_UNCERTAINTY_HZ, 0.0),
}

GOLD = np.asarray(Gold().generate(SF), np.uint8)
#: The payload, one bit per symbol: doppler's own PN, so the truth the BER is
#: scored against is exactly what the synth sent. It outlasts the capture.
PAYLOAD = PN(seed=1, length=15).generate(N_SYM + 64)


def make_capture(start_hz: float, rate_hz_s: float, seed: int) -> np.ndarray:
    """The capture: noise alone, then the payload on the Gold code through
    coupled Doppler, with noise throughout.

    :class:`~doppler.wfm.Synth` in continuous DSSS mode spreads
    :data:`PAYLOAD` at :data:`SYM_RATE`, clean.
    :class:`~doppler.impairment.DopplerChannel` then applies the Doppler that
    starts at ``start_hz`` and changes at ``rate_hz_s``: it resamples the
    signal to stretch the chip clock and rotates the carrier, both from the
    one ppm figure, ``Hz / CARRIER_HZ``. The noise is
    :class:`~doppler.source.AWGN`, added after the channel, with the
    per-component sigma :func:`~doppler.wfm.wfm_awgn_amplitude` gives for the
    Es/N0 over one symbol's ``FS / SYM_RATE`` samples.

    Parameters
    ----------
    start_hz : float
        Carrier Doppler at the first signal sample, Hz.
    rate_hz_s : float
        Its rate of change, Hz/s.
    seed : int
        The noise draw.

    Returns
    -------
    x : NDArray[np.complex64]
        :data:`LEAD_IN` samples of noise, then the signal plus noise.
    """
    clean = Synth(
        type="dsss",
        fs=FS,
        sps=SPC,
        data_code=GOLD.tobytes(),
        symbol_rate=SYM_RATE,
        data=PAYLOAD.tobytes(),
    ).steps(int(N_SYM * FS / SYM_RATE))
    # The synth sends the payload once, then silence: had it run dry, the end
    # of the capture would be no signal at all.
    assert np.mean(np.abs(clean[-TE:]) ** 2) > 0.5, (
        "the payload ran out before the capture ended"
    )
    channel = DopplerChannel(
        fs=FS,
        carrier_hz=CARRIER_HZ,
        doppler_ppm=start_hz / CARRIER_HZ * 1e6,
        doppler_rate_ppm_s=rate_hz_s / CARRIER_HZ * 1e6,
    )
    x = np.concatenate(
        [np.zeros(LEAD_IN, np.complex64), channel.execute(clean)]
    )
    amp = float(
        wfm_awgn_amplitude(ESN0_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    return (x + AWGN(seed=seed, amplitude=amp).generate(len(x))).astype(
        np.complex64
    )


# --8<-- [end:signal]

from doppler.ber import (  # noqa: E402
    BerMeter,
    ber_esn0_db_for_ser,
    ber_evm_db,
    ber_lock_symbol,
)
from doppler.dsss import AsyncDsssReceiver  # noqa: E402
from doppler.snr import snr_m2m4_db  # noqa: E402

#: One row per push, read from the receiver after the push.
ROW = np.dtype(
    [
        ("state", "U9"),  # lifecycle: searching, refining or tracking
        ("locked", bool),  # the symbol lock
        ("metric", float),  # the lock metric the flag thresholds
        ("doppler_hz", float),  # the live carrier estimate, Hz
        ("symbols", int),  # symbols that push emitted
    ]
)


# --8<-- [start:receiver]
def receive(x: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Feed the capture one code epoch per push, and read the receiver after
    each one.

    The knobs, and why each is set:

    - ``cn0_dbhz`` sizes the detectors for the link's signal level.
    - ``doppler_uncertainty`` is one-sided: the search covers the carrier
      within that many Hz of nominal, in either direction.
    - ``carrier_freq_hz`` couples the code rate to the tracked carrier, which
      a Doppler that stretches the chip clock needs.
    - ``lost_confirm_s`` with ``reacquire``: a track whose symbol lock stays
      down that long is released, and the receiver searches again.

    Everything else is the receiver's default.

    Returns
    -------
    symbols : NDArray[np.complex64]
        Every symbol ``steps()`` emitted, in order.
    rows : NDArray
        One :data:`ROW` per push.
    """
    rx = AsyncDsssReceiver(
        code=GOLD,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=CN0_DBHZ,
        doppler_uncertainty=CARRIER_UNCERTAINTY_HZ,
        carrier_freq_hz=CARRIER_HZ,
        lost_confirm_s=LOST_CONFIRM_S,
        reacquire=True,
    )
    rows = np.zeros(len(x) // TE, ROW)
    symbols = []
    for k in range(len(rows)):
        block = rx.steps(x[k * TE : (k + 1) * TE])
        st = rx.status()
        rows[k] = (
            rx.lifecycle,
            st.locked,
            st.lock_metric,
            st.doppler_hz,
            len(block),
        )
        symbols.append(block)
    return np.concatenate(symbols).astype(np.complex64), rows


# --8<-- [end:receiver]


# --8<-- [start:validate]
#: The checks' bounds. Each is a physical statement, not a fit to this run.
#: Symbol lock is a per-draw outcome on random noise, so one draw in eight may
#: miss without the receiver having changed; a missed hand-off that stays
#: missed (no ``reacquire``) fails it by several draws.
MIN_LOCK_FRACTION = 7 / 8
#: The pooled BER, quoted as dB of Es/N0 coherent BPSK would need for the same
#: errors. The receiver holds about 1 dB. A pool of a few dozen errors
#: measures the rate to about 20%, which near 8 dB is about 0.25 dB of Es/N0
#: (the curve falls about fourfold per dB there), so 1.5 dB clears the
#: counting noise twice over and fails a BER twice the receiver's.
MAX_LOSS_DB = 1.5
#: EVM normalizes by the measured total power, so a perfect receiver reads
#: 10*log10(N/(S+N)), not -Es/N0. A locked stream sits above it by about
#: 0.9 dB of EVM, the implementation loss the despread no longer hides since
#: its output stopped flattering it (#2495), and the EVM of a few thousand
#: symbols counts to about 0.1 dB: EVM_TOL_DB clears both and fails an EVM
#: twice the loss.
EVM_IDEAL_DB = -10.0 * np.log10(1.0 + 10.0 ** (ESN0_DB / 10.0))
EVM_TOL_DB = 1.5
LAG_SPAN = 1000  # symbols the stream may start into the payload


def score(meter: BerMeter, symbols: np.ndarray, rows: np.ndarray) -> dict:
    """Score one draw, adding its errors to ``meter``.

    The measurement window starts where the symbol lock is sustained
    (:func:`~doppler.ber.ber_lock_symbol` over the per-symbol flag). The
    receiver emits nothing while it searches and refines, so the stream starts
    some way into the payload; the meter detects that offset from a marker
    placed inside the window, at most :data:`LAG_SPAN` symbols in. BER and EVM
    are scored on the same window.

    Returns, for a draw that locked: how many hand-offs were released before
    the one that locked; that hand-off's pull-in, from the start of tracking to
    the first symbol lock; the time from the signal's arrival to the sustained
    lock; and the window's EVM and M2M4, in dB.
    """
    assert len(symbols) <= len(PAYLOAD), "more symbols than the payload holds"
    flags = np.repeat(rows["locked"], rows["symbols"]).astype(np.uint8)
    lo = ber_lock_symbol(flags)
    if lo < 0:
        return {"locked": False}
    assert meter.align(symbols, t0=lo + LAG_SPAN, lag_span=LAG_SPAN), (
        "the payload marker was not detected in the locked stream"
    )
    meter.score(symbols, lo=lo, hi=len(symbols))

    # Pushes are the time base: the push that emitted symbol `lo`, the first
    # push with the lock up, and the start of the track it came up on.
    sustained = np.searchsorted(np.cumsum(rows["symbols"]), lo, side="right")
    first_lock = int(np.argmax(rows["locked"]))
    tracking = rows["state"][: first_lock + 1] == "tracking"
    track_start = int(np.flatnonzero(~tracking)[-1]) + 1
    return {
        "locked": True,
        "released": int(np.sum(tracking[:-1] & ~tracking[1:])),
        "pull_in_s": (first_lock - track_start) * TE / FS,
        "lock_s": ((sustained + 1) * TE - LEAD_IN) / FS,
        "evm_db": float(ber_evm_db(symbols, lo, len(symbols), 2)),
        "m2m4_db": float(snr_m2m4_db(symbols[lo:])),
    }


def check(
    draws: list[dict], meter: BerMeter, start_hz: float, rate_hz_s: float
) -> dict:
    """Assert one regime's outcome over its draws, and return its summary.

    - At least :data:`MIN_LOCK_FRACTION` of the draws reach symbol lock.
    - Every hand-off that locked pulled in within :data:`LOST_CONFIRM_S`, so
      the release rule cut none short.
    - On a ramp that crosses zero inside the capture, every draw locked before
      the crossing, so the receiver holds lock through it.
    - The pooled BER is within :data:`MAX_LOSS_DB` of coherent BPSK, and no
      better than it, which no receiver can be.
    - Every locked draw's EVM is within :data:`EVM_TOL_DB` of
      :data:`EVM_IDEAL_DB`, and its M2M4 clears the input Es/N0 less the loss
      budget.
    """
    locked = [d for d in draws if d["locked"]]
    fraction = len(locked) / len(draws)
    assert fraction >= MIN_LOCK_FRACTION, (
        f"symbol lock on {len(locked)} of {len(draws)} draws"
    )
    max_pull_in_s = max(d["pull_in_s"] for d in locked)
    assert max_pull_in_s < LOST_CONFIRM_S, (
        f"a pull-in of {max_pull_in_s:.2f} s reaches the confirm time"
    )
    zero_s = -start_hz / rate_hz_s if rate_hz_s else -1.0
    if 0.0 < zero_s < DURATION_S:
        late = max(d["lock_s"] for d in locked)
        assert late < zero_s, (
            f"locked {late:.2f} s after arrival, after the zero crossing at "
            f"{zero_s:.2f} s"
        )
    ber = meter.ber()
    loss_db = ESN0_DB - ber_esn0_db_for_ser(2, ber.p_hat)
    # Written so a NaN (a rate off the theory curve: no decode) fails.
    assert loss_db <= MAX_LOSS_DB, f"implementation loss {loss_db:.2f} dB"
    assert not ber_esn0_db_for_ser(2, ber.hi) > ESN0_DB, (
        f"BER {ber.p_hat:.2e} is below coherent BPSK's: the measurement is "
        f"broken"
    )
    for d in locked:
        assert abs(d["evm_db"] - EVM_IDEAL_DB) <= EVM_TOL_DB, (
            f"EVM {d['evm_db']:.1f} dB against {EVM_IDEAL_DB:.1f} dB ideal"
        )
        assert d["m2m4_db"] >= ESN0_DB - MAX_LOSS_DB, (
            f"M2M4 {d['m2m4_db']:.1f} dB: not a locked stream"
        )
    return {
        "locked": len(locked),
        "draws": len(draws),
        "first_try": sum(d["released"] == 0 for d in locked),
        "max_pull_in_s": max_pull_in_s,
        "max_lock_s": max(d["lock_s"] for d in locked),
        "ber": ber,
        "loss_db": loss_db,
        "evm_db": float(np.mean([d["evm_db"] for d in locked])),
        "m2m4_db": float(np.mean([d["m2m4_db"] for d in locked])),
    }


# --8<-- [end:validate]


def run_regime(start_hz: float, rate_hz_s: float):
    """Every draw of one regime, scored into one meter. Also returns the first
    locked draw's symbols, rows and score, for the figure."""
    meter = BerMeter(m=2)
    meter.set_truth(PAYLOAD)
    draws, first = [], None
    for seed in DRAWS:
        symbols, rows = receive(make_capture(start_hz, rate_hz_s, seed))
        draws.append(score(meter, symbols, rows))
        if first is None and draws[-1]["locked"]:
            first = (symbols, rows, draws[-1])
    return check(draws, meter, start_hz, rate_hz_s), draws, first


def main(out_path: str = "async_dsss_receiver_spec_demo.png") -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    results = {}
    for name, (start_hz, rate_hz_s) in REGIMES.items():
        summary, draws, first = run_regime(start_hz, rate_hz_s)
        results[name] = (summary, draws, first)
        b = summary["ber"]
        print(
            f"{name}: symbol lock on {summary['locked']}/{summary['draws']} "
            f"draws, {summary['first_try']} on the first hand-off; pull-in "
            f"{summary['max_pull_in_s']:.2f} s at most, locked "
            f"{summary['max_lock_s']:.2f} s after arrival at most; BER "
            f"{b.p_hat:.1e} ({b.errors} errors in {b.symbols} bits), "
            f"{summary['loss_db']:.1f} dB from "
            f"coherent BPSK; EVM {summary['evm_db']:.1f} dB; M2M4 "
            f"{summary['m2m4_db']:.1f} dB"
        )

    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))
    syms, rows, shown = results["TCA crossing"][2]
    start_hz, rate_hz_s = REGIMES["TCA crossing"]
    t = ((np.arange(len(rows)) + 1) * TE - LEAD_IN) / FS
    tracking = rows["state"] == "tracking"

    # The live carrier against the Doppler the channel applied.
    b.plot(
        t,
        np.where(t >= 0, start_hz + rate_hz_s * t, np.nan),
        color="k",
        lw=1.4,
        ls="--",
        label="applied Doppler",
    )
    b.plot(
        t,
        np.where(tracking, rows["doppler_hz"], np.nan),
        color="#1f77b4",
        lw=0.9,
        label="status().doppler_hz",
    )
    b.set_title("TCA crossing: the live carrier through zero", fontsize=9)
    b.set_xlabel("time from arrival (s)")
    b.set_ylabel("carrier (Hz)")
    b.legend(fontsize=7)
    b.grid(alpha=0.25)

    # The lock metric, the lock flag, and where the receiver was searching.
    d.plot(t, rows["metric"], lw=1.0, color="#1f77b4", label="lock_metric")
    d.fill_between(
        t,
        -0.1,
        1.1,
        where=rows["locked"],
        color="#2ca02c",
        alpha=0.2,
        step="post",
        label="locked",
    )
    d.fill_between(
        t,
        -0.1,
        1.1,
        where=~tracking,
        color="0.6",
        alpha=0.25,
        step="post",
        label="searching or refining",
    )
    d.set_ylim(-0.1, 1.1)
    d.set_title("TCA crossing: symbol lock", fontsize=9)
    d.set_xlabel("time from arrival (s)")
    d.set_ylabel("lock metric")
    d.legend(fontsize=7, loc="lower right")
    d.grid(alpha=0.25)

    # The constellation over the measured window.
    lo = ber_lock_symbol(
        np.repeat(rows["locked"], rows["symbols"]).astype(np.uint8)
    )
    w = syms[lo:]
    a.scatter(w.real, w.imag, s=3, alpha=0.3, color="#1f77b4")
    lim = 1.3 * float(np.percentile(np.abs(w), 99))
    a.set_xlim(-lim, lim)
    a.set_ylim(-lim, lim)
    a.set_aspect("equal")
    a.axhline(0, color="k", lw=0.5)
    a.axvline(0, color="k", lw=0.5)
    a.set_title(
        f"TCA crossing: {len(w)} symbols after symbol lock\n"
        f"EVM {shown['evm_db']:.1f} dB",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    # Time to symbol lock, every draw of every regime.
    for i, (name, (_, draws, _)) in enumerate(results.items()):
        color = ("#1f77b4", "#d62728", "#2ca02c")[i]
        c.plot([], [], "s", color=color, label=name)  # the legend's swatch
        for seed, dr in zip(DRAWS, draws):
            if not dr["locked"]:
                continue
            c.plot(
                seed + 0.2 * (i - 1),
                dr["lock_s"],
                "o",
                color=color,
                mfc=color if dr["released"] == 0 else "none",
            )
    c.set_title(
        "Time to symbol lock per draw\n"
        "(filled: first hand-off; hollow: after searching again)",
        fontsize=9,
    )
    c.set_xlabel("noise draw")
    c.set_ylabel("seconds from arrival")
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

    fig.suptitle(
        f"AsyncDsssReceiver: Gold-{SF}, {CHIP_RATE / 1e6:.3f} Mcps, "
        f"{SYM_RATE:.0f} bps, {CARRIER_HZ / 1e9:.1f} GHz carrier, "
        f"Es/N0={ESN0_DB:.0f} dB",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(
        sys.argv[1]
        if len(sys.argv) > 1
        else "async_dsss_receiver_spec_demo.png"
    )
