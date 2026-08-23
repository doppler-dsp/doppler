"""ema_demo.py — choosing an EMA coefficient, and decimating without retuning.

`ema_step(state, x, alpha)` is the running average underneath every
estimator in doppler — power detectors, lock statistics, spectrum traces.
The only decision a caller makes is `alpha`, and this demo shows the three
things that decision is actually about, each measured rather than asserted.

1. **Memory.**  How long the average remembers, in observations.  A step
   response crosses `1 - 1/e` of the way to its new level on exactly
   sample `ceil(-1/ln(1-alpha))` — the discrete law, not an approximation
   to `1/alpha`.
2. **Noise reduction.**  What the memory buys: a white input of variance
   `s2` converges to `s2 * alpha/(2-alpha)`, so the estimator's SNR
   improves by `(2-alpha)/alpha`.  This is the law `det_ema_alpha`
   INVERTS to size a coefficient for a requested detector SNR, so it is
   load-bearing rather than decorative.
3. **Decimation.**  A loop that updates once per chunk of `d` samples must
   not thereby change its own time constant.  `ema_alpha_decim(alpha, d)`
   is the coefficient that advances `d` samples in one step, and it is
   EXACT at `d == 1` — which is what makes `decim` a throughput knob
   rather than a retune.  The naive `1-(1-alpha)**d` is not: it cancels
   catastrophically, by 26865 ulps at `alpha = 1e-5`.

Every panel is a claim from `docs/design/ema.md` and
`native/tests/test_util_core.c`, drawn at the Python face to show the
binding delivers what the C certifies.

Saves a three-panel plot to ema_memory.png:
  - left   : step responses for four alphas, with the 1/e crossing marked
  - middle : measured vs predicted noise reduction, over a decade of alpha
  - right  : the compounding error of the naive form against this one, in
             ulps, at d = 1 — the panel is empty of orange because the
             shipped path is exact everywhere

Run:
  python ema_demo.py
"""

import math

import matplotlib

matplotlib.use("Agg")  # headless: render straight to a file, no display

import matplotlib.pyplot as plt

# --8<-- [start:memory]
import numpy as np

from doppler.util import ema_alpha_decim, ema_step

# A step from 0 to 1: the state climbs toward the observation and crosses
# 1 - 1/e of the way on sample ceil(-1/ln(1-alpha)).
alpha = 0.05
state = 0.0
memory = 0  # observations to the 1/e point — the answer we are after
while state < 1.0 - 1.0 / math.e:
    state = ema_step(state, 1.0, alpha)
    memory += 1
# memory == 20 for alpha = 0.05, i.e. ceil(-1/ln(1-alpha))
# --8<-- [end:memory]

ALPHAS = (0.5, 0.1, 0.05, 0.01)
RNG_SEED = 7

print("=== 1. memory — samples to the 1/e point ===")
steps = {}
crossings = {}
for a in ALPHAS:
    n_show = max(8, int(6.0 / a))
    s = 0.0
    trace = np.empty(n_show)
    cross = None
    for i in range(n_show):
        s = ema_step(s, 1.0, a)
        trace[i] = s
        if cross is None and s >= 1.0 - 1.0 / math.e:
            cross = i + 1  # 1-based: the sample that crossed
    steps[a] = trace
    crossings[a] = cross
    predicted = math.ceil(-1.0 / math.log1p(-a))
    print(
        f"  alpha {a:<6}: crossed on sample {cross:>4}, "
        f"ceil(-1/ln(1-alpha)) = {predicted:>4}"
    )
    # The discrete law is EXACT, not approximate. An earlier draft of this
    # claim scored the crossing against the continuous 1/alpha within 2%
    # and failed at alpha = 0.5 (2 against 1.44) — the law was fine, the
    # claim was not. A crossing is an integer.
    assert cross == predicted, (
        f"alpha {a}: crossed on {cross}, law says {predicted}"
    )

print()
print("=== 2. noise reduction — the law det_ema_alpha inverts ===")
rng = np.random.default_rng(RNG_SEED)
noise_alphas = (0.2, 0.05, 0.01, 0.001)
measured, predicted_var = [], []
for a in noise_alphas:
    # Converge first, then measure: the transient is not the steady state.
    x = rng.standard_normal(400_000)
    s = 0.0
    burn = int(10.0 / a)
    for v in x[:burn]:
        s = ema_step(s, float(v), a)
    tail = np.empty(len(x) - burn)
    for i, v in enumerate(x[burn:]):
        s = ema_step(s, float(v), a)
        tail[i] = s
    m = float(np.var(tail))
    p = a / (2.0 - a)  # unit-variance input
    measured.append(m)
    predicted_var.append(p)
    print(
        f"  alpha {a:<6}: measured var {m:.5f}, predicted {p:.5f}, "
        f"ratio {m / p:.3f}  (SNR gain {10 * math.log10(1 / m):.1f} dB)"
    )
    # 5% is the band the validation report uses; a 400k-sample estimate of
    # a variance is itself noisy, and quoting tighter would be quoting the
    # seed rather than the law.
    assert abs(m / p - 1.0) < 0.05, (
        f"alpha {a}: measured var {m:.6f} against predicted {p:.6f}"
    )

print()
print("=== 3. decimation — exact at d = 1, where it must be ===")


def ulps(a: float, b: float) -> int:
    """Distance in representable doubles — the unit an exactness claim
    belongs in, since a relative error hides how many values lie between."""
    ia = int(np.frombuffer(np.float64(a).tobytes(), dtype=np.int64)[0])
    ib = int(np.frombuffer(np.float64(b).tobytes(), dtype=np.int64)[0])
    return abs(ia - ib)


decim_alphas = (1e-9, 1e-7, 1e-5, 6.25e-5, 1e-3, 0.01, 0.05, 0.5)
naive_ulps, shipped_ulps = [], []
for a in decim_alphas:
    shipped = ema_alpha_decim(a, 1)
    naive = 1.0 - (1.0 - a) ** 1
    naive_ulps.append(ulps(naive, a))
    shipped_ulps.append(ulps(shipped, a))
    print(
        f"  alpha {a:<9}: naive off by {ulps(naive, a):>9} ulps, "
        f"shipped off by {ulps(shipped, a)}"
    )
    # The property the whole decimation story rests on: at d = 1 the
    # compounded coefficient IS alpha, bit for bit.
    assert shipped == a, f"alpha {a}: ema_alpha_decim(alpha, 1) != alpha"

# And compounding is exact, not merely close: d steps of alpha reach the
# same state as one step of the compounded coefficient.
worst = 0.0
for a in (1e-4, 1e-3, 0.01, 0.05):
    for d in (2, 8, 32, 128):
        s = 0.0
        for _ in range(d):
            s = ema_step(s, 1.0, a)
        one = ema_step(0.0, 1.0, ema_alpha_decim(a, d))
        worst = max(worst, abs(s - one))
print(f"  d steps of alpha vs one compounded step: worst |diff| {worst:.1e}")
assert worst < 1e-15, f"compounding is not exact: worst diff {worst:.2e}"

# The naive form must actually be BAD, or panel 3 shows two flat lines and
# the comparison proves nothing.
assert max(naive_ulps) > 1000, (
    f"the naive form's worst error is only {max(naive_ulps)} ulps — this "
    f"panel would not be showing a difference worth drawing"
)

fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(13, 4.2))

for a in ALPHAS:
    t = steps[a]
    n_ax = np.arange(1, len(t) + 1) * a  # time in 1/alpha units
    ax1.plot(n_ax, t, lw=1.4, label=f"alpha {a}")
    ax1.plot(crossings[a] * a, t[crossings[a] - 1], "o", ms=4, color="0.3")
ax1.axhline(1.0 - 1.0 / math.e, color="0.6", ls="--", lw=1, label="$1 - 1/e$")
ax1.set_xlabel(r"observations $\times\ \alpha$")
ax1.set_ylabel("state")
ax1.set_title(
    "1. Memory: the $1/e$ crossing\n"
    "is exactly $\\lceil -1/\\ln(1-\\alpha)\\rceil$"
)
ax1.legend(fontsize=8)
ax1.grid(alpha=0.3)

ax2.loglog(
    noise_alphas, predicted_var, "k--", lw=1, label=r"$\alpha/(2-\alpha)$"
)
ax2.loglog(noise_alphas, measured, "o", ms=6, label="measured")
ax2.set_xlabel(r"$\alpha$")
ax2.set_ylabel("converged variance (unit-variance input)")
ax2.set_title("2. Noise reduction:\nthe law det_ema_alpha inverts")
ax2.legend(fontsize=8)
ax2.grid(alpha=0.3, which="both")

ax3.loglog(
    decim_alphas,
    np.maximum(naive_ulps, 0.5),
    "o-",
    lw=1.4,
    color="tab:orange",
    label=r"naive $1-(1-\alpha)^d$",
)
ax3.loglog(
    decim_alphas,
    np.maximum(shipped_ulps, 0.5),
    "s-",
    lw=1.4,
    color="tab:blue",
    label="ema_alpha_decim",
)
ax3.axhline(0.5, color="0.6", ls="--", lw=1)
ax3.text(decim_alphas[1], 0.62, "exact", fontsize=8, color="0.35")
ax3.set_xlabel(r"$\alpha$")
ax3.set_ylabel("error at $d=1$ (ulps, 0 plotted at 0.5)")
ax3.set_title(
    "3. Compounding: exact at $d=1$,\nwhere the answer must be $\\alpha$"
)
ax3.legend(fontsize=8)
ax3.grid(alpha=0.3, which="both")

fig.tight_layout()
out_path = "ema_memory.png"
fig.savefig(out_path, dpi=120)
print(f"\nwrote {out_path}")
print(f"(alpha = 0.05 remembers {memory} observations to the 1/e point)")
