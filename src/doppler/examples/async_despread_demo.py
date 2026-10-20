"""async_despread_demo.py -- :class:`doppler.track.Dll` as the despreader of a
DSSS link whose data clock is asynchronous to its code.

``Dll(segments=K)`` takes a carrier-wiped stream, removes the spreading code,
and emits ``K`` despread samples per code period. It tracks the code with an
early/late power discriminator and reports lock from an always-on detector.
When the data clock is asynchronous to the code, a data transition lands in
most code periods. With ``segments > 1`` the loop steers on the
transition-free window its look-back finds, and its output carries the
symbol clock to the next stage.

The motion that shifts the carrier also dilates the code clock. A carrier
offset ``f`` on a carrier ``fc`` is a code-rate ratio of ``f / fc``, so the
carrier estimate that wipes the carrier also says how fast the code runs.
Handed to the loop as ``set_rate_aid(f / fc)``, it carries the Doppler, and
the loop carries only what the estimate missed. That is what lets the loop
stay narrow.

This demo runs one link twice, unaided and aided, at two Dopplers:

- **0.6 ppm, this link's Doppler**: a 1500 Hz carrier offset on a 2.5 GHz
  carrier. Both loops hold lock. Unaided, the loop filter output settles at
  the dilation: the loop carries it. Aided, it settles at zero. Both read up
  to one NCO step high (below).
- **5 ppm, inflated** to about eight times this link's, to show what the aid
  is for. The narrow loop cannot carry it unaided and loses lock within tens
  of milliseconds. Aided, it holds.

A locked loop's output is the data along one axis, four samples per code
period and about four and a half per symbol; the figure's bottom panel shows
the aided loop's at 0.6 ppm. A loop off the code puts out noise.

The loop filter output is ``code_rate - 1``, in chips per chip, without the
aid. The code NCO commands its rate in whole phase-increment steps of
``SF * spc / 2**32`` chips per chip (0.48 ppm here) and truncates, so the
filter output reads high, by up to one step, against the rate the loop
carries. At this link's Doppler one step is nearly the whole Doppler, so the
code loop's rate is not how to measure the Doppler. The carrier estimate is.

When a run does not behave like this one, read three things in order:
``locked`` (the code loop's lock flag), then ``lock_stat`` (the statistic it
thresholds), then ``code_rate - 1``. A filter output that walks away while
``lock_stat`` falls is a loop asked to carry more rate than its bandwidth
allows. Aid it, or widen ``bn``.

The design is in ``docs/design/async-dsss-receiver.md`` section 3.

Run:  python -m doppler.examples.async_despread_demo  [out.png]
"""

import sys

# --8<-- [start:signal]
import numpy as np

from doppler.detection import (
    det_cn0_to_snr,
    det_n_noncoh,
    det_threshold_noncoherent,
    det_verify_count,
)
from doppler.impairment import DopplerChannel
from doppler.source import AWGN, LO
from doppler.track import Dll
from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

# ── Setup: this scenario's numbers ─────────────────────────────────────────
SF = 1023  # Gold code period, chips (CCSDS)
CHIP_RATE = 3.069e6  # chips per second
SPC = 2  # samples per chip
FS = CHIP_RATE * SPC  # sample rate, 6.138 MSa/s
SYM_RATE = 2700.0  # BPSK symbols per second, asynchronous to the code
TE = SF * SPC  # one code period, samples: one push
EPOCH_S = TE / FS  # seconds of signal per push
CARRIER_HZ = 2.5e9  # the RF carrier the motion shifts
CARRIER_OFFSET_HZ = 1500.0  # this link's carrier offset
LINK_PPM = 1e6 * CARRIER_OFFSET_HZ / CARRIER_HZ  # the same motion: 0.6 ppm
INFLATED_PPM = 5.0  # inflated, to show what the aid is for
ESN0_DB = 10.0  # Es/N0 at the receiver input
CN0_DBHZ = ESN0_DB + 10.0 * np.log10(SYM_RATE)  # C/N0, dB-Hz
DURATION_S = 0.6  # seconds of signal per run

GOLD = np.asarray(Gold().generate(SF), np.uint8)

#: The code NCO's rate step, chips per chip: one phase increment of its
#: 32-bit accumulator, which turns once per code period.
NCO_STEP = TE / 2**32


def receive(doppler_ppm, seed=2):
    """The link at a Doppler: the emitter through a channel, then noise.

    The emitter is :class:`~doppler.wfm.Synth` in continuous DSSS mode on
    nominal frequency, with its own PRBS payload. The
    :class:`~doppler.impairment.DopplerChannel` applies the motion: it
    dilates every clock in the stream by ``1 + doppler_ppm * 1e-6`` and
    shifts the carrier by ``CARRIER_HZ * doppler_ppm * 1e-6``, from the one
    parameter. The noise is :class:`~doppler.source.AWGN` at the Es/N0.

    Returns
    -------
    x : NDArray[np.complex64]
        The received stream.
    channel : DopplerChannel
        The channel. Its ``offset_hz`` stands in for the carrier estimate an
        upstream carrier loop hands over, and its ``delay_samples`` for the
        code phase acquisition hands over.
    """
    n = int(DURATION_S / EPOCH_S) * TE
    tx = np.asarray(
        Synth(
            type="dsss",
            fs=FS,
            sps=SPC,
            data_code=GOLD.tobytes(),
            symbol_rate=SYM_RATE,
        ).steps(n),
        np.complex64,
    )
    channel = DopplerChannel(
        fs=FS, carrier_hz=CARRIER_HZ, doppler_ppm=doppler_ppm
    )
    # The channel takes at most 65536 samples a call.
    rx = np.concatenate(
        [channel.execute(tx[i : i + 65536]) for i in range(0, n, 65536)]
    )
    amp = float(
        wfm_awgn_amplitude(ESN0_DB - 10.0 * np.log10(FS / SYM_RATE), 1.0)
    )
    noise = AWGN(seed=seed, amplitude=amp).generate(len(rx))
    return (rx + noise).astype(np.complex64), channel


# --8<-- [end:signal]


# --8<-- [start:track]
BN = 0.002  # narrow, for a quiet code phase: what the aid lets the loop keep
SEGMENTS = (
    4  # despread samples per code period, for an asynchronous data clock
)
PFA = 1e-3  # the lock detector's false-alarm rate per decision
PD = 0.99  # its detection rate per decision, at the link's C/N0

# One row per push, read from the loop's own properties.
ROW = np.dtype(
    [
        ("locked", bool),
        ("lock_stat", float),
        ("filter_out", float),
        ("samples", int),
    ]
)


def despreader(init_chip):
    """A Dll for this link, its lock detector sized for the link.

    Given the symbol period, the detector's looks are coherent over the
    transition-free part of a symbol rather than one despread sample.
    ``det_n_noncoh`` gives the looks a decision needs for ``PD`` at the C/N0,
    and ``det_verify_count`` the consecutive misses that hold false drops
    under 1e-6 per decision. ``AsyncDsssReceiver`` sizes its own the same
    way, because at its defaults, one look per despread sample, the flag
    drops on a loop that holds the code: measured on the receiver, a few
    times a second at Es/N0 10.7 dB and 96% of the time at 5.7 dB
    (``docs/design/async-dsss-receiver-measurements.md`` section 12.3).

    Returns the Dll and its lock threshold.
    """
    dll = Dll(GOLD, sps=SPC, init_chip=init_chip, bn=BN, segments=SEGMENTS)
    dll.set_symbol_period(SEGMENTS * CHIP_RATE / (SF * SYM_RATE))
    look = dll.symbol_window * (TE // SEGMENTS)  # samples per look
    n_looks = det_n_noncoh(det_cn0_to_snr(CN0_DBHZ, FS), look, PD, PFA, 4000)
    dll.configure_lock(PFA, n_looks)
    dll.set_lock_verify(
        det_verify_count(PFA, PFA * 1e-3), det_verify_count(1.0 - PD, 1e-6)
    )
    return dll, det_threshold_noncoherent(PFA, n_looks)


def track(x, channel, aided):
    """Wipe the carrier, then despread with a Dll, one code period per push.

    One carrier estimate does two jobs. It wipes the carrier, and divided by
    the carrier frequency it is the code-rate aid. The loop starts at the code
    phase acquisition reports, to one sample. The code phase runs opposite to
    the delay, so a code that arrives ``L`` chips late starts the loop at
    ``SF - L``.

    Returns one :data:`ROW` per push, the lock threshold, and the despread
    output: ``SEGMENTS`` samples per code period, the data with the code
    removed.
    """
    carrier_estimate_hz = channel.offset_hz
    wiped = x * LO(-carrier_estimate_hz / FS).steps(len(x))

    lag_chips = np.floor(channel.delay_samples) / SPC
    dll, threshold = despreader(SF - lag_chips)
    if aided:
        dll.set_rate_aid(carrier_estimate_hz / CARRIER_HZ)

    rows = np.zeros(len(wiped) // TE, ROW)
    output = []
    for k in range(len(rows)):
        despread = dll.steps(wiped[k * TE : (k + 1) * TE])
        output.append(despread)
        rows[k] = (
            dll.locked,
            dll.lock_stat,
            dll.code_rate - 1.0,  # the loop filter output, chips per chip
            len(despread),
        )
    return rows, threshold, np.concatenate(output)


# --8<-- [end:track]


# --8<-- [start:validate]
def check(runs, outputs):
    """Assert what the runs show, and return what this run measured.

    ``runs`` maps ``(doppler_ppm, aided)`` to a run's rows, and ``outputs``
    to its despread output.

    Three bounds come from the object and the link, not the draw. The
    despread output is ``SEGMENTS`` samples per code period. A locked loop's
    output is the data along one axis, at whatever carrier phase the data
    sits: its power along that axis stands above its power across it by up
    to the link budget's ``1 + 2 * SNR`` per output sample, and reads below
    it because a sample straddling a data flip loses amplitude. The check
    takes half the budget as the line, below which the output is
    noise. And the NCO truncates its
    command, so a locked loop's settled filter output reads between the rate
    the loop carries and one NCO step above it. The check widens that by a
    quarter step either side, an allowance for the noise left in a mean over
    the settled half that is not part of the bound. The rest describes this
    run: first lock within a generous 50 ms, which loops held lock, and when
    the unaided loop lost it.
    """
    ms = 1e3 * EPOCH_S
    got = {}
    first = {}
    for (ppm, aided), rows in runs.items():
        assert abs(rows["samples"].sum() - SEGMENTS * len(rows)) <= SEGMENTS, (
            "the despread output is not SEGMENTS samples per code period"
        )
        assert rows["locked"].any(), f"{ppm} ppm aided={aided}: never locked"
        first[ppm, aided] = int(np.argmax(rows["locked"]))
        assert first[ppm, aided] * ms < 50.0, (
            f"{ppm} ppm aided={aided}: first lock at "
            f"{first[ppm, aided] * ms:.0f} ms"
        )

    # This run: aided, the loop holds lock from its first lock at both
    # Dopplers; unaided it holds this link's, and loses the inflated one.
    for key in ((LINK_PPM, True), (INFLATED_PPM, True), (LINK_PPM, False)):
        assert runs[key]["locked"][first[key] :].all(), (
            f"this run: {key[0]} ppm aided={key[1]} lost lock"
        )
    unaided = runs[INFLATED_PPM, False]["locked"]
    assert not unaided[len(unaided) // 2 :].any(), (
        "this run: the unaided loop held the inflated Doppler"
    )
    got["inflated_unaided_lost_ms"] = (
        len(unaided) - int(np.argmax(unaided[::-1]))
    ) * ms

    # The output: the data along one axis while the loop holds the code,
    # noise once it has lost it. The two eigenvalues of the output's I/Q
    # second moments are its power along its strongest axis and across it,
    # whatever carrier phase the data sits at. Per output sample the SNR is
    # Es/N0 over the samples a symbol spans.
    per_symbol = SEGMENTS * CHIP_RATE / (SF * SYM_RATE)
    budget = 1.0 + 2.0 * 10.0 ** (ESN0_DB / 10.0) / per_symbol
    for (ppm, aided), out in outputs.items():
        settled = out[len(out) // 2 :]
        iq = np.stack([settled.real, settled.imag]).astype(np.float64)
        across, along = np.linalg.eigvalsh(iq @ iq.T / iq.shape[1])
        ratio = float(along / across)
        holds = (ppm, aided) != (INFLATED_PPM, False)
        assert (ratio > budget / 2) == holds, (
            f"{ppm} ppm aided={aided}: output power along its axis against "
            f"across it {ratio:.2f}, against the budget's {budget:.2f}"
        )
        got[f"{ppm}_{aided}_axis"] = ratio
    got["axis_budget"] = budget

    # Physical: a locked loop's settled filter output is the rate it carries,
    # read high by up to one NCO step. Unaided, the loop carries the
    # dilation. Aided, the aid carries it and the loop carries nothing.
    for ppm, aided in (
        (LINK_PPM, False),
        (LINK_PPM, True),
        (INFLATED_PPM, True),
    ):
        rows = runs[ppm, aided]
        carried = 0.0 if aided else ppm * 1e-6
        mean = float(rows["filter_out"][len(rows) // 2 :].mean())
        steps = (mean - carried) / NCO_STEP
        assert -0.25 <= steps <= 1.25, (
            f"{ppm} ppm aided={aided}: filter output {1e6 * mean:+.2f} ppm, "
            f"{steps:+.2f} NCO steps from the {1e6 * carried:.2f} ppm the "
            "loop carries"
        )
        got[f"{ppm}_{aided}_filter_ppm"] = 1e6 * mean
    return got


# --8<-- [end:validate]


def main(out_path="async_despread_demo.png"):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    runs, outputs = {}, {}
    for ppm in (LINK_PPM, INFLATED_PPM):
        x, channel = receive(ppm)
        for aided in (False, True):
            runs[ppm, aided], eta, outputs[ppm, aided] = track(
                x, channel, aided
            )
    got = check(runs, outputs)
    print(
        f"this run, NCO step {1e6 * NCO_STEP:.2f} ppm. At {LINK_PPM:.1f} ppm "
        f"both loops held lock; the settled filter output read "
        f"{got[f'{LINK_PPM}_False_filter_ppm']:+.2f} ppm unaided, "
        f"{got[f'{LINK_PPM}_True_filter_ppm']:+.2f} ppm aided. At the "
        f"inflated {INFLATED_PPM:.0f} ppm the unaided loop lost lock "
        f"{got['inflated_unaided_lost_ms']:.0f} ms in; aided it held, the "
        f"filter output at {got[f'{INFLATED_PPM}_True_filter_ppm']:+.2f} ppm."
        f" The output's power along its axis against across it, budget "
        f"{got['axis_budget']:.1f}: {got[f'{LINK_PPM}_True_axis']:.1f} aided "
        f"at {LINK_PPM:.1f} ppm, {got[f'{INFLATED_PPM}_False_axis']:.1f} "
        f"unaided at {INFLATED_PPM:.0f} ppm"
    )

    colours = {False: "#d95f02", True: "#1b6ec2"}
    names = {False: "unaided", True: "aided"}
    # A 30 ms running mean: the timescale the narrow loop moves on.
    win = max(1, round(0.03 / EPOCH_S))

    fig = plt.figure(figsize=(11, 9.0))
    grid = fig.add_gridspec(3, 2, height_ratios=(1.0, 1.0, 0.9))
    axes = [
        [fig.add_subplot(grid[0, 0]), fig.add_subplot(grid[0, 1])],
        [fig.add_subplot(grid[1, 0]), fig.add_subplot(grid[1, 1])],
    ]
    for top, bottom in zip(axes[0], axes[1]):
        top.sharex(bottom)
    for (lock_ax, filt_ax), ppm, label in zip(
        axes,
        (LINK_PPM, INFLATED_PPM),
        (
            f"{LINK_PPM:.1f} ppm, this link",
            f"{INFLATED_PPM:.0f} ppm, inflated",
        ),
    ):
        for aided in (False, True):
            rows = runs[ppm, aided]
            t_ms = np.arange(len(rows)) * EPOCH_S * 1e3
            lock_ax.plot(
                t_ms,
                rows["lock_stat"],
                lw=0.9,
                color=colours[aided],
                label=names[aided],
            )
            filt = 1e6 * rows["filter_out"]
            filt_ax.plot(t_ms, filt, lw=0.5, alpha=0.3, color=colours[aided])
            filt_ax.plot(
                t_ms[win - 1 :],
                np.convolve(filt, np.ones(win) / win, mode="valid"),
                lw=1.5,
                color=colours[aided],
                label=f"{names[aided]}, 30 ms mean",
            )
        lock_ax.axhline(eta, color="k", ls="--", lw=1.0, label="threshold")
        lock_ax.set_ylabel("lock_stat")
        lock_ax.set_title(f"{label}: code lock statistic", fontsize=9)
        lock_ax.legend(fontsize=7, loc="lower right")
        lock_ax.grid(alpha=0.25)

        # Where each loop reads when it carries its rate: that rate, plus up
        # to one NCO step.
        for aided, carried in ((False, ppm), (True, 0.0)):
            filt_ax.axhspan(
                carried,
                carried + 1e6 * NCO_STEP,
                color=colours[aided],
                alpha=0.12,
                lw=0,
            )
        filt_ax.axhline(ppm, color="0.3", ls="--", lw=1.0, label="dilation")
        filt_ax.axhline(0.0, color="0.3", lw=0.6)
        filt_ax.set_ylim(-1.5, ppm + 2.5)
        filt_ax.set_ylabel("code_rate - 1 (ppm)")
        filt_ax.set_title(f"{label}: loop filter output", fontsize=9)
        filt_ax.legend(fontsize=7, loc="upper right")
        filt_ax.grid(alpha=0.25)
    for ax in axes[1]:
        ax.set_xlabel("time (ms)")

    # What the despreader emits: the aided loop's output at this link's
    # Doppler, a stretch from the settled half, SEGMENTS samples per code
    # period and a non-whole number per symbol, so each symbol's sign spans
    # four or five samples. The data lands on I only because this demo wipes
    # the carrier with the channel's exact offset and the channel adds no
    # carrier phase; an estimated carrier leaves its residual phase on it.
    out_ax = fig.add_subplot(grid[2, :])
    out = outputs[LINK_PPM, True]
    per_symbol = SEGMENTS * CHIP_RATE / (SF * SYM_RATE)
    first = len(out) // 2
    shown = np.arange(first, first + round(14 * per_symbol))
    out_ax.plot(shown, out[shown].imag, ".", ms=3, color="0.6", label="Q")
    out_ax.plot(
        shown,
        out[shown].real,
        "-o",
        ms=3.5,
        lw=0.8,
        color=colours[True],
        label="I",
    )
    out_ax.axhline(0.0, color="0.3", lw=0.6)
    out_ax.set_xlim(shown[0] - 1, shown[-1] + 1)
    out_ax.set_xlabel(f"despread sample ({SEGMENTS} per code period)")
    out_ax.set_ylabel("output")
    out_ax.set_title(
        f"{LINK_PPM:.1f} ppm, aided: the despread output, "
        f"{per_symbol:.2f} samples a symbol",
        fontsize=9,
    )
    out_ax.legend(fontsize=7, loc="upper right")
    out_ax.grid(alpha=0.25)

    fig.suptitle(
        f"track.Dll, unaided and aided: Gold-{SF}, "
        f"{CHIP_RATE / 1e6:.3f} Mcps, {SYM_RATE:.0f} bps, "
        f"Es/N0={ESN0_DB:.0f} dB (bands: carried rate + one NCO step)",
        fontsize=10,
    )
    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "async_despread_demo.png")
