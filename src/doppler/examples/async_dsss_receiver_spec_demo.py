"""async_dsss_receiver_spec_demo.py -- one
:class:`~doppler.dsss.AsyncDsssReceiver` on the CCSDS 415.1-B-1 forward link.

The link: BPSK at 3,000 symbols a second, spread by a 1,023-chip Gold code
whose chip rate is the carrier times 31/21216, the data asynchronous to the
code. The receiver takes 11 MSa/s, searches a Doppler within +/-1,500 Hz that
moves by up to 400 Hz/s, refines it, tracks, and emits symbols. Told the RF
carrier (``carrier_freq_hz``), it feeds the carrier offset its carrier loop
tracks to the code loop as the matching code-rate offset, so the code loop
follows the chip clock the Doppler stretches.

The demo runs at both ends of the band, 2.2 and 2.8 GHz, at Es/N0 = 10 dB,
6 dB above the link's 4 dB floor, so the despread symbols show as two
clusters. It demonstrates the receiver; it does not certify it. The checks
say it acquired, locked and decoded. Performance at the floor is measured by
the receiver's validators
(``docs/design/async-dsss-receiver-measurements.md``).

Run:  python -m doppler.examples.async_dsss_receiver_spec_demo  [out.png]
"""

from __future__ import annotations

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.examples._extra import require
from doppler.impairment import DopplerChannel
from doppler.resample import RateConverter
from doppler.source import AWGN
from doppler.wfm import PN, Gold, Synth, wfm_awgn_amplitude

# ── Setup: the 415.1-B-1 forward link ──────────────────────────────────────
SF = 1023  # Gold code period, chips
SYM_RATE = 3000.0  # BPSK symbols per second, asynchronous to the code
FS = 11e6  # the receiver's sample rate
CODE_ONLY_SYM = 275  # each frame opens with this many data-free symbols
FRAME_SYM = 1000  # symbols a frame
DOPPLER_UNCERTAINTY_HZ = 1500.0  # one-sided: within +/-1,500 Hz
DOPPLER_RATE_HZ_S = 400.0  # the fastest the Doppler changes
#: Es/N0 at the receiver input: 6 dB above the link's 4 dB floor, so the
#: despread symbols show as two clusters. A demonstration, not a validator.
ESN0_DB = 10.0
CN0_DBHZ = ESN0_DB + 10.0 * np.log10(SYM_RATE)  # the same, as C/N0 in dB-Hz
# Symbol lock down this long: release, search again.
LOST_CONFIRM_S = 0.4

PUSH = 4096  # samples a push into the receiver
CHUNK = 8192  # emitter samples a pass through the rate converter
DURATION_S = 2.5  # the signal, each capture
N_SYM = int(DURATION_S * SYM_RATE)
LEAD_IN = 20 * PUSH + 737  # noise alone first; not a whole number of pushes
DRAWS = range(1, 5)  # one noise seed per draw
#: The Doppler at the signal's first sample; it ramps at DOPPLER_RATE_HZ_S
#: through zero at START_HZ / -DOPPLER_RATE_HZ_S = 1.25 s, to +500 Hz.
START_HZ = -500.0

#: The two carriers, the band's ends, as (carrier Hz; chip rate, the carrier
#: times 31/21216; the rate converter's ratio, FS over the emitter's two
#: samples a chip; the Doppler at the start and its rate, in ppm of the
#: carrier, the unit the channel takes). The derived numbers are LITERALS,
#: the C twin's own: a division written in each face rounds differently
#: under the C twin's -ffast-math, and a ulp of rate moves a bit error
#: between the twins. Checked against their definitions below.
REGIMES = {
    "2.2 GHz": (
        2.2e9,
        3214555.052790347,
        1.7109677419354838,
        -0.22727272727272727,
        0.18181818181818182,
    ),
    "2.8 GHz": (
        2.8e9,
        4091251.8853695323,
        1.344331797235023,
        -0.17857142857142858,
        0.14285714285714285,
    ),
}
for _fc, _chip, _ratio, _ppm, _ppm_s in REGIMES.values():
    assert abs(_chip - _fc * 31.0 / 21216.0) < 1e-6
    assert abs(_ratio - FS / (2.0 * _chip)) < 1e-12
    assert abs(_ppm * _fc * 1e-6 - START_HZ) < 1e-6
    assert abs(_ppm_s * _fc * 1e-6 - DOPPLER_RATE_HZ_S) < 1e-6

GOLD = np.asarray(Gold().generate(SF), np.uint8)
#: The payload, one bit per symbol: doppler's own PN, each frame's first
#: CODE_ONLY_SYM symbols zero (the data-free window the search looks in), so
#: the truth the BER is scored against is exactly what the emitter sent.
_PN = PN(seed=1, length=15).generate(N_SYM + 64)
PAYLOAD = np.where(
    np.arange(len(_PN)) % FRAME_SYM < CODE_ONLY_SYM, 0, _PN
).astype(np.uint8)


class Stimulus:
    """One capture, streamed a push at a time: noise alone, then the payload
    on the Gold code through the Doppler, with noise throughout.

    :class:`~doppler.wfm.Synth` in continuous DSSS mode spreads
    :data:`PAYLOAD` at two samples a chip, clean.
    :class:`~doppler.resample.RateConverter` takes it to :data:`FS`, and
    :class:`~doppler.impairment.DopplerChannel` applies the Doppler: it
    resamples the signal to stretch the chip clock and rotates the carrier,
    both from one ppm figure. The noise is :class:`~doppler.source.AWGN`,
    added after the channel, with the per-component sigma
    :func:`~doppler.wfm.wfm_awgn_amplitude` gives for the Es/N0 over one
    symbol's ``FS / SYM_RATE`` samples. Nothing is held but a chunk, so a
    capture costs no more memory than a push.
    """

    def __init__(self, regime: tuple, seed: int) -> None:
        fc, chip, ratio, ppm, ppm_s = regime
        self.syn = Synth(
            type="dsss",
            fs=2.0 * chip,
            sps=2,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
            data=PAYLOAD.tobytes(),
        )
        self.rc = RateConverter(rate=ratio)
        self.ch = DopplerChannel(
            fs=FS, carrier_hz=fc, doppler_ppm=ppm, doppler_rate_ppm_s=ppm_s
        )
        amp = wfm_awgn_amplitude(ESN0_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
        self.noise = AWGN(seed=seed, amplitude=float(amp))
        self.lead = LEAD_IN
        self.clean_left = int(N_SYM * 2.0 * chip / SYM_RATE)
        self.stage = np.zeros(0, np.complex64)

    def next(self) -> np.ndarray | None:
        """The next push, or None once the signal ends inside one."""
        parts, need = [], PUSH
        if self.lead:
            z = min(self.lead, need)
            parts.append(np.zeros(z, np.complex64))
            self.lead -= z
            need -= z
        while need:
            if not len(self.stage):
                if not self.clean_left:
                    return None
                n = min(self.clean_left, CHUNK)
                self.clean_left -= n
                clean = self.syn.steps(n)
                self.last_power = float(np.mean(np.abs(clean) ** 2))
                self.stage = self.ch.execute(self.rc.execute(clean))
                continue
            take = min(need, len(self.stage))
            parts.append(self.stage[:take])
            self.stage = self.stage[take:]
            need -= take
        return (np.concatenate(parts) + self.noise.generate(PUSH)).astype(
            np.complex64
        )


# --8<-- [end:signal]

from doppler.ber import BerMeter, ber_evm_db, ber_lock_symbol
from doppler.dsss import AsyncDsssReceiver
from doppler.snr import snr_m2m4_db

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
def receive(regime: tuple, seed: int) -> tuple[np.ndarray, np.ndarray]:
    """Stream one capture through the receiver, one push at a time, and read
    the receiver after each push.

    The knobs, and why each is set:

    - ``cn0_dbhz`` sizes the detectors for the link's signal level.
    - ``doppler_uncertainty`` is one-sided: the search covers the carrier
      within that many Hz of nominal, in either direction.
    - ``carrier_freq_hz`` couples the code rate to the tracked carrier, which
      a Doppler that stretches the chip clock needs.
    - ``code_only_s`` is the data-free window that opens each frame, which
      the search integrates across, and ``doppler_rate`` bounds how far the
      Doppler moves while it does.
    - ``lost_confirm_s`` with ``reacquire``: a track whose symbol lock stays
      down that long is released, and the receiver searches again.
    - ``carrier_window="code_period"``: the carrier loop forms its error over
      one coherent code-period window instead of each partial. It holds
      because a symbol, 333 us, is at least a code period, 250 to 318 us
      across the band, so some window of one period is free of data
      transitions.

    Everything else is the receiver's default.

    Returns
    -------
    symbols : NDArray[np.complex64]
        Every symbol ``steps()`` emitted, in order.
    rows : NDArray
        One :data:`ROW` per push.
    """
    fc, chip = regime[0], regime[1]
    rx = AsyncDsssReceiver(
        code=GOLD,
        chip_rate=chip,
        symbol_rate=SYM_RATE,
        sample_rate=FS,
        cn0_dbhz=CN0_DBHZ,
        doppler_uncertainty=DOPPLER_UNCERTAINTY_HZ,
        carrier_freq_hz=fc,
        lost_confirm_s=LOST_CONFIRM_S,
        reacquire=True,
        code_only_s=CODE_ONLY_SYM / SYM_RATE,
        doppler_rate=DOPPLER_RATE_HZ_S,
        carrier_window="code_period",
    )
    cap = Stimulus(regime, seed)
    rows, symbols = [], []
    while (x := cap.next()) is not None:
        block = rx.steps(x)
        st = rx.status()
        rows.append(
            (
                rx.lifecycle,
                st.locked,
                st.lock_metric,
                st.doppler_hz,
                len(block),
            )
        )
        symbols.append(block)
    # The synth sends the payload once, then silence: had it run dry, the
    # end of the capture would be no signal at all.
    assert cap.last_power > 0.5, "the payload ran out before the run ended"
    return np.concatenate(symbols).astype(np.complex64), np.array(rows, ROW)


# --8<-- [end:receiver]


# --8<-- [start:validate]
#: The checks' bounds. This is a demonstration, run well above the link's
#: floor so the symbols show; floor performance is certified elsewhere
#: (docs/design/async-dsss-receiver-measurements.md). Symbol lock is a
#: per-draw outcome on random noise, so one draw in four may miss.
MIN_LOCK_FRACTION = 3 / 4
#: "Decoded": the pooled BER over the locked windows. A carrier here measures
#: at most a few errors in about 28,000 bits; the bound passes that with room
#: and fails a stream that is not decoding (BER near 0.5).
MAX_BER = 1e-3
LAG_SPAN = 1000  # symbols the stream may start into the payload


def marker_at(lo: int) -> int:
    """Where the BER meter's marker starts in the payload: the first symbol
    at least :data:`LAG_SPAN` into the locked stream that is data, never a
    frame's data-free window, which is all one symbol and so marks nothing.
    """
    t0 = lo + LAG_SPAN
    if t0 % FRAME_SYM < CODE_ONLY_SYM:
        t0 += CODE_ONLY_SYM - t0 % FRAME_SYM
    return t0


def score(meter: BerMeter, symbols: np.ndarray, rows: np.ndarray) -> dict:
    """Score one draw, adding its errors to ``meter``.

    The measurement window starts where the symbol lock is sustained
    (:func:`~doppler.ber.ber_lock_symbol` over the per-symbol flag). The
    receiver emits nothing while it searches and refines, so the stream starts
    some way into the payload; the meter detects that offset from a marker,
    :func:`marker_at`. BER and EVM are scored on the same window.

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
    assert meter.align(symbols, t0=marker_at(lo), lag_span=LAG_SPAN), (
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
        "pull_in_s": (first_lock - track_start) * PUSH / FS,
        "lock_s": ((sustained + 1) * PUSH - LEAD_IN) / FS,
        "evm_db": float(ber_evm_db(symbols, lo, len(symbols), 2)),
        "m2m4_db": float(snr_m2m4_db(symbols[lo:])),
    }


def check(draws: list[dict], meter: BerMeter) -> dict:
    """Assert one carrier's outcome over its draws, and return its summary.

    - At least :data:`MIN_LOCK_FRACTION` of the draws reach symbol lock.
    - Every hand-off that locked pulled in within :data:`LOST_CONFIRM_S`, so
      the release rule cut none short.
    - Every draw locked before the Doppler's zero crossing, so the receiver
      holds the lock through it.
    - The pooled BER is at most :data:`MAX_BER`: the frames decode.
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
    zero_s = -START_HZ / DOPPLER_RATE_HZ_S
    late = max(d["lock_s"] for d in locked)
    assert late < zero_s, (
        f"locked {late:.2f} s after arrival, after the zero crossing at "
        f"{zero_s:.2f} s"
    )
    ber = meter.ber()
    # Written so a NaN (no symbols scored: no decode) fails.
    assert ber.p_hat <= MAX_BER, f"BER {ber.p_hat:.2e} over {MAX_BER:.0e}"
    return {
        "locked": len(locked),
        "draws": len(draws),
        "first_try": sum(d["released"] == 0 for d in locked),
        "max_pull_in_s": max_pull_in_s,
        "max_lock_s": late,
        "ber": ber,
        "evm_db": float(np.mean([d["evm_db"] for d in locked])),
        "m2m4_db": float(np.mean([d["m2m4_db"] for d in locked])),
    }


# --8<-- [end:validate]


def run_regime(regime: tuple):
    """Every draw at one carrier, scored into one meter. Also returns the
    first locked draw's symbols, rows and score, for the figure."""
    meter = BerMeter(m=2)
    meter.set_truth(PAYLOAD)
    draws, first = [], None
    for seed in DRAWS:
        symbols, rows = receive(regime, seed)
        draws.append(score(meter, symbols, rows))
        if first is None and draws[-1]["locked"]:
            first = (symbols, rows, draws[-1])
    return check(draws, meter), draws, first


def main(out_path: str = "async_dsss_receiver_spec_demo.png") -> None:
    require("matplotlib")
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    results = {}
    for name, regime in REGIMES.items():
        summary, draws, first = run_regime(regime)
        results[name] = (summary, draws, first)
        b = summary["ber"]
        print(
            f"{name}: symbol lock on {summary['locked']}/{summary['draws']} "
            f"draws, {summary['first_try']} on the first hand-off; pull-in "
            f"{summary['max_pull_in_s']:.2f} s at most, locked "
            f"{summary['max_lock_s']:.2f} s after arrival at most; BER "
            f"{b.p_hat:.1e} ({b.errors} errors in {b.symbols} bits); "
            f"EVM {summary['evm_db']:.1f} dB; M2M4 "
            f"{summary['m2m4_db']:.1f} dB"
        )

    fig, ((a, b), (c, d)) = plt.subplots(2, 2, figsize=(11, 8.5))
    name = "2.2 GHz"
    syms, rows, shown = results[name][2]
    t = ((np.arange(len(rows)) + 1) * PUSH - LEAD_IN) / FS
    tracking = rows["state"] == "tracking"

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
        f"{name}: {len(w)} symbols after symbol lock\n"
        f"EVM {shown['evm_db']:.1f} dB",
        fontsize=9,
    )
    a.set_xlabel("I")
    a.set_ylabel("Q")
    a.grid(alpha=0.25)

    # The live carrier against the Doppler the channel applied.
    b.plot(
        t,
        np.where(t >= 0, START_HZ + DOPPLER_RATE_HZ_S * t, np.nan),
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
    b.set_title(f"{name}: the live carrier through zero", fontsize=9)
    b.set_xlabel("time from arrival (s)")
    b.set_ylabel("carrier (Hz)")
    b.legend(fontsize=7)
    b.grid(alpha=0.25)

    # Time to symbol lock, every draw at both carriers.
    for i, (rname, (_, draws, _)) in enumerate(results.items()):
        color = ("#1f77b4", "#d62728")[i]
        c.plot([], [], "s", color=color, label=rname)  # the legend's swatch
        for seed, dr in zip(DRAWS, draws):
            if not dr["locked"]:
                continue
            c.plot(
                seed + 0.15 * (2 * i - 1),
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
    c.set_xticks(list(DRAWS))
    c.set_ylabel("seconds from arrival")
    c.set_ylim(bottom=0)
    c.legend(fontsize=7)
    c.grid(alpha=0.25)

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
    d.set_title(f"{name}: symbol lock", fontsize=9)
    d.set_xlabel("time from arrival (s)")
    d.set_ylabel("lock metric")
    d.legend(fontsize=7, loc="lower right")
    d.grid(alpha=0.25)

    fig.suptitle(
        f"AsyncDsssReceiver on CCSDS 415.1-B-1: Gold-{SF}, "
        f"{SYM_RATE:.0f} sym/s, 11 MSa/s, a {DOPPLER_RATE_HZ_S:.0f} Hz/s "
        f"ramp through zero, Es/N0={ESN0_DB:.0f} dB",
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
