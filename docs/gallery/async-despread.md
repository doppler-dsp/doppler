# Streaming Async Despreader

![Async despreader: oversampled BPSK out](../assets/async_despread_demo.png)

A [`track.Dll`](../api/python-track.md) in **segments** mode — the streaming
DSSS despreader. Its one job is to **remove the PN code and output samples**.
The data symbols ride on a clock that is *asynchronous* to the code epoch; that
is merely why it despreads in `K` sub-epoch **partial** correlations, so a
mid-epoch data flip cannot collapse the code-tracking discriminator. Carrier
recovery and symbol-timing recovery are **downstream** problems — the despreader
leaves the residual carrier on its output. SF = 127 chips, 8 samples/chip,
`segments = 4`, a small residual carrier, and a data clock offset 4e‑3 from the
code epoch.

## What you're seeing

**Left — Oversampled asynchronous BPSK out.** The despread partial stream at
`K = 4` samples/symbol (after a downstream carrier wipe, for clarity). The symbol
edges (red, dashed) **slide** through the code epochs (gray, dotted) because the
symbol clock is independent of the code clock — that is the "async" in the name.
The dips at the edges are the single partial that straddles each data transition;
every other partial is a clean ±1.

**Middle — Carrier rides on the output.** The same partials in the complex plane,
*without* the carrier wipe: the two BPSK clusters are smeared into a **ring** by
the residual carrier. The despreader does not touch it — a downstream
[`track.Costas`](costas.md) loop collapses the ring back to ±1.

**Right — Code stays locked under the carrier.** The DLL's non-coherent
`(|E| − |L|)` discriminator works on *envelopes*, so it is **carrier-blind**: the
code-rate estimate rings in and settles onto the true code Doppler with the
residual carrier still on the samples.

## How it works

`Dll(segments=K)` splits each code epoch into `K` partial integrate-and-dumps.
Each partial despreads `TE/K` samples against the local code; the `K` partials
per epoch are an oversampled view of the symbol (≈ `K` samples/symbol when the
symbol rate is near the code rate). Code tracking folds each partial's early/late
envelopes into a **non-coherent** epoch sum `Σ|E_k|`, `Σ|L_k|` — a data flip
changes a partial's *sign*, not its *magnitude*, so only the one straddling
segment degrades.

```python
from doppler.track import Dll

# code: 0/1 chips for one period; sps samples/chip; segments = partials/epoch
d = Dll(code, sps=8, init_chip=0.0, bn=0.002, zeta=0.707, spacing=0.5,
        segments=4)
part = d.steps(rx)        # oversampled async BPSK out (PN removed)
rate = d.code_rate        # tracked code rate (carrier-blind)

# downstream — carrier + symbol recovery on the partials:
from doppler.track import Costas, SymbolSync
# Costas(...).steps(part) -> SymbolSync(...).steps(...) -> bits
```

A short partial window also makes the despread **carrier-tolerant**: for a
½-Doppler-bin residual after acquisition the integrate-and-dump loss is only
~0.2 dB at `K = 4` (versus ~3.9 dB for a full-epoch `segments = 1` prompt), so
the residual carrier rides out on the output for the downstream loop instead of
eroding the despread. `steps()` is block-size invariant and returns an
independent array per call, so a receiver can stream blocks and keep every one.

Source: `src/doppler/examples/async_despread_demo.py`.
See also the design note *Async Symbol Despreader* and the carrier-free
two-clock study `async_despreader_study.py`.
