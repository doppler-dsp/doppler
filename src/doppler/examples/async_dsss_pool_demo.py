"""async_dsss_pool_demo.py -- the multi-emitter pool as ONE object: two
emitters on one code arrive, one leaves and is released, it returns and
is a new detection -- the lifecycle :class:`~doppler.dsss.AsyncDsssPool`
holds (``docs/design/async-dsss-receiver.md`` section 8.2), with the
event log attached and a timeline of who held which slot when.

The C twin (``native/examples/async_dsss_pool_demo.c``) manages the
lifecycle the binding hides -- the symbol buffer, the borrowed log, the
slot keyed by both coordinates. This page shows the *result*: the slots'
occupancy against the truth, each tracked receiver's live Doppler, and
the log's transitions in order.

**The waveform is not built here.** Each emitter is the shipped
continuous-DSSS :class:`~doppler.wfm.Synth` (the code from
:class:`~doppler.wfm.Gold`, asynchronous BPSK from its own seeded PN) at
its own carrier offset, and the noise is a ``type="noise"`` Synth scaled
by :func:`~doppler.wfm.wfm_awgn_amplitude` from the C/N0 -- nothing here
spreads a chip, draws a bit or a sigma.

**A slot is the emitter's by both coordinates, never by a count.** The
searcher's false alarms are part of the lifecycle: at pfa 1e-3 a noise
peak seeds a free slot every few hundred milliseconds, refines to
nothing and is released one interval later. So an emitter's slot is the
one whose SEED is within the searcher's row of its Doppler AND within a
chip of its code phase at the seed's sample -- the synth's own clock --
and the figure colours every other assignment as what it is.
"""

from __future__ import annotations

import json
import os
import sys
import tempfile

# --8<-- [start:geometry]
import numpy as np

from doppler.dsss import AsyncDsssPool
from doppler.telemetry import EventLog
from doppler.wfm import Gold, Synth, wfm_awgn_amplitude

SF = 1023
CHIP_RATE = 5.0e6
SPC = 2
FS = CHIP_RATE * SPC
SYM_RATE = 2700.0
TE = SF * SPC  # one code epoch, the push block
CN0_DBHZ = 47.0
DU = 6000.0  # the searcher's span: +-6 kHz, D = 1 rows of 4.9 kHz
LOST_S = 0.3  # the release interval, short enough to see
N_SLOTS = 4
EMITTERS = {  # name: (Doppler Hz, code phase at sample 0 in chips, seed)
    "A": (1500.0, 0, 1),
    "B": (-3500.0, 900, 2),
}
ON_S, OFF_S = 1.0, 3.0 * LOST_S  # A always on; B on, off, on
CODE = np.asarray(Gold().generate(SF)).astype(np.uint8)
# --8<-- [end:geometry]


# --8<-- [start:stimulus]
def emitter(doppler_hz: float, chip0: int, seed: int) -> Synth:
    """One emitter: the shipped continuous DSSS at a carrier offset, clean,
    its code `chip0` chips in at the stream's first sample (a burn-in the
    caller discards) -- two emitters must differ in code phase as well as
    Doppler, since one phase is one peak to the searcher."""
    syn = Synth(
        type="dsss",
        data_code=bytes(CODE.tolist()),
        symbol_rate=SYM_RATE,
        sps=SPC,
        snr=100.0,  # clean: the noise is added once, at the sum
        fs=FS,
        freq=doppler_hz,
        seed=seed,
    )
    if chip0:
        syn.steps(chip0 * SPC)
    return syn


def stimulus(seed: int = 7):
    """Both emitters on for ON_S, B off for OFF_S (its synth keeps running:
    coming back is not restarting), both on again; noise at the sum.
    Returns the capture and B's on-air mask per epoch."""
    a = emitter(*EMITTERS["A"])
    b = emitter(*EMITTERS["B"])
    n_on, n_off = int(ON_S * FS / TE), int(OFF_S * FS / TE)
    on_b = np.array([1] * n_on + [0] * n_off + [1] * n_on, dtype=bool)
    blocks = []
    for on in on_b:
        sa = a.steps(TE)
        sb = b.steps(TE)
        blocks.append(sa + sb if on else sa)
    x = np.concatenate(blocks)
    # C/N0 referred to fs; the Synth emits unit total power, the amplitude
    # is per component, and the sqrt(2) bridges the two conventions.
    amp = float(wfm_awgn_amplitude(CN0_DBHZ - 10.0 * np.log10(FS), 1.0))
    noise = Synth(type="noise", fs=FS, seed=seed).steps(len(x))
    return (x + amp * np.sqrt(2.0) * noise).astype(np.complex64), on_b


# --8<-- [end:stimulus]


# --8<-- [start:pool]
def owner(pool: AsyncDsssPool, slot: int) -> str | None:
    """Which emitter a slot's SEED is -- row and chip phase -- or None
    for a false alarm (a noise seed at another phase)."""
    r = pool.status(slot)
    if not r.assigned:
        return None
    for name, (f, chip0, _) in EMITTERS.items():
        truth = (r.seed_sample / SPC + chip0) % SF
        dc = abs(r.seed_chip_phase - truth)
        dc = min(dc, SF - dc)
        if abs(r.seed_doppler_hz - f) <= pool.doppler_res_hz and dc <= 1.0:
            return name
    return "false alarm"


def run(x: np.ndarray, events_path: str):
    pool = AsyncDsssPool(
        CODE,
        chip_rate=CHIP_RATE,
        symbol_rate=SYM_RATE,
        spc=SPC,
        cn0_dbhz=CN0_DBHZ,
        pfa=1e-3,
        doppler_uncertainty=DU,
        n_slots=N_SLOTS,
        lost_confirm_s=LOST_S,
        threads=1,
    )
    log = EventLog(events_path)
    pool.set_event_log(log)
    n_ep = len(x) // TE
    who = np.full((N_SLOTS, n_ep), "", dtype=object)  # owner per epoch
    dopp = np.full((N_SLOTS, n_ep), np.nan)  # live Doppler while tracking
    n_syms = dict.fromkeys(EMITTERS, 0)
    for k in range(n_ep):
        pool.push(x[k * TE : (k + 1) * TE])
        for i in range(N_SLOTS):
            o = owner(pool, i)
            r = pool.status(i)
            who[i, k] = o or ""
            if o in n_syms:
                n_syms[o] += len(pool.symbols(i))
            if r.assigned and r.state == 2:  # tracking
                dopp[i, k] = r.doppler_hz
    pool.set_event_log(None)
    log.close()
    with open(events_path) as fh:
        events = [json.loads(line) for line in fh]
    assert len(events) == pool.events, "every transition reached the log"
    return who, dopp, n_syms, events


# --8<-- [end:pool]


def check(who, on_b, n_syms, events) -> None:
    """The lifecycle, asserted against the stimulus's truth."""
    t = np.arange(who.shape[1]) * TE / FS
    held = {n: (who == n).sum(axis=0) for n in EMITTERS}  # slots per epoch
    # Assigned once: never two slots on one emitter.
    for n in EMITTERS:
        assert held[n].max() == 1, f"{n} was held by two slots at once"
    # A: tracked for the whole run once found; B: found in each stint.
    first_a = np.argmax(held["A"] > 0)
    assert held["A"][first_a:].mean() > 0.99, "A was not held throughout"
    off = np.flatnonzero(~on_b)
    t_off, t_back = t[off[0]], t[off[-1] + 1]
    b_before = held["B"][: off[0]]
    assert b_before.max() == 1, "B was never assigned in its first stint"
    # Released by the rule after the departure -- inside the interval plus
    # half a second, never before it -- and a new slot after the return.
    rel = np.flatnonzero(held["B"][off[0] :] == 0)
    assert len(rel), "B's slot was never released"
    t_rel = t[off[0] + rel[0]] - t_off
    assert LOST_S <= t_rel <= LOST_S + 0.5, f"released at {t_rel:.2f} s"
    slot_before = int(np.flatnonzero(who[:, off[0] - 1] == "B")[0])
    back = np.flatnonzero(held["B"][off[-1] + 1 :] > 0)
    assert len(back), "B was not re-acquired on return"
    slot_after = int(np.flatnonzero(who[:, off[-1] + 1 + back[0]] == "B")[0])
    assert n_syms["A"] > 1000 and n_syms["B"] > 1000, "symbols by slot"
    labels = [e["core:label"] for e in events]
    want = ["seeded", "tracking", "lost", "released", "seeded"]
    i = 0
    for lab in labels:
        if i < len(want) and lab == want[i]:
            i += 1
    assert i == len(want), f"the log's order: {labels}"
    print(
        f"A: one slot from {t[first_a]:.3f} s to the end; "
        f"B: slot {slot_before} until it left at {t_off:.2f} s, released "
        f"{t_rel * 1e3:.0f} ms later (the interval is {LOST_S * 1e3:.0f} ms), "
        f"slot {slot_after} after it returned at {t_back:.2f} s; "
        f"{n_syms['A']} + {n_syms['B']} symbols; {len(events)} events"
    )


def figure(who, dopp, on_b, out_path: str) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    t = np.arange(who.shape[1]) * TE / FS
    colours = {"A": "tab:blue", "B": "tab:orange", "false alarm": "tab:gray"}
    fig, (ax0, ax1) = plt.subplots(
        2, 1, figsize=(9, 5.5), sharex=True, constrained_layout=True
    )
    seen: set[str] = set()
    for i in range(N_SLOTS):
        for name, c in colours.items():
            m = who[i] == name
            if not m.any():
                continue
            ax0.fill_between(
                t,
                i - 0.4,
                i + 0.4,
                where=m,
                color=c,
                step="mid",
                label=None if name in seen else name,
            )
            seen.add(name)
        ax1.plot(t, dopp[i], lw=1, label=f"slot {i}")
    off = np.flatnonzero(~on_b)
    for ax in (ax0, ax1):
        ax.axvline(t[off[0]], color="k", ls="--", lw=0.8)
        ax.axvline(t[off[-1] + 1], color="k", ls="--", lw=0.8)
    ax0.set_yticks(range(N_SLOTS))
    ax0.set_ylabel("slot")
    ax0.set_title(
        "AsyncDsssPool: who holds which slot -- B leaves at the first "
        "dashed line, returns at the second"
    )
    ax0.legend(loc="upper right", fontsize=8)
    for f, _, _ in EMITTERS.values():
        ax1.axhline(f, color="k", lw=0.5, alpha=0.4)
    ax1.set_ylabel("live Doppler (Hz)")
    ax1.set_xlabel("time (s)")
    ax1.legend(loc="upper right", fontsize=8)
    fig.savefig(out_path, dpi=120)
    plt.close(fig)


def main(out_path: str = "async_dsss_pool_demo.png") -> None:
    x, on_b = stimulus()
    with tempfile.TemporaryDirectory() as d:
        who, dopp, n_syms, events = run(x, os.path.join(d, "run.events"))
    check(who, on_b, n_syms, events)
    figure(who, dopp, on_b, out_path)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main(*sys.argv[1:2])
