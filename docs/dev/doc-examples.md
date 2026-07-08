# Doc examples — every snippet is tested

Every Python code example in the docs is executed in CI. This is not a
convention you have to remember — it is **enforced by discovery**: a gate scans
every page under `docs/` and runs its Python fences, so a new page is covered
the moment it exists. There is no opt-in list. The gate lives in
`src/doppler/tests/test_doc_snippets.py`; run it locally with:

```sh
uv run pytest -m docs_snippets
```

## Why

Doc snippets rot silently. The quickstart once showed `HalfbandDecimator()`;
the constructor later gained a required argument, the example started raising
`TypeError`, and nothing noticed for weeks. Prose examples are the first thing a
new user runs — they must always work.

## The four states of a fence

Every ```` ```python ```` / ```` ```pycon ```` block in `docs/` is in exactly
one of these. They **layer**: `exec` proves the snippet still *runs*; the other
three also prove it still shows the *right result*.

| State       | How                                  | Proves                                 | Use for                          |
| ----------- | ------------------------------------ | -------------------------------------- | -------------------------------- |
| **exec**    | plain ```` ```python ````            | the gate runs it                       | it *runs*                        |
| **doctest** | a `>>>` session                      | the gate checks output                 | it runs *and* the value is right |
| **include** | `--8<--` from a tested `.py`         | byte-identical to code CI already runs | zero drift, by construction      |
| **skip**    | `<!-- docs-snippet: skip=REASON -->` | nothing (documents *why*)              | —                                |

Pseudocode is not a state — write it as ```` ```text ```` so it is out of scope.

## Runnable-first — how to choose

**A skipped block is not tested, so it can rot exactly like an unguarded one.**
The whole point of the gate is to *run* the code, so reach for `skip=` last, not
first. When a block fails the gate, work down this list — stop at the first that
fits:

1. **`--8<--` from a tested example script** — the gold standard. Almost every
    gallery page mirrors a `src/doppler/examples/*_demo.py` that already runs in
    CI; pull the code in and the shown code *is* the tested code, forever.
1. **exec with one line of real setup** — most "fragment" failures are a real
    API call one undefined name away from running (`c.steps(rx)` where `rx` is
    never defined). Define `rx` for real (`rx = LO(0.05).steps(4096)`) and the
    block genuinely tests `steps`. Do **not** invent a fake value that merely
    silences the error — a block passing against a bogus `rx` hides drift instead
    of catching it.
1. **doctest** — same, but when the *value* is the point.
1. **`skip=`** — only when the code genuinely cannot run headless: a blocking
    `recv()`, a hardware source, a two-terminal demo, or a read of a capture file
    the reader supplies. A `steps()`/`accumulate()`/`push()` call is **not** one
    of these — make it runnable instead.

> **Anti-pattern:** do not add a hidden shared setup namespace so fragments "just
> run". A block that passes against an injected global you can't see is worse
> than untested — it looks green while masking a rename. Keep shown code and run
> code identical; that is what `--8<--` guarantees for free.

### exec — the default for inline prose

Just write a normal fenced block. **A page is one notebook**: its fences share a
namespace and run top to bottom, so a later block may use names an earlier block
bound. The gate runs in a throwaway working directory (writing a file is safe)
with `numpy.random.seed(0)` for determinism and a per-block timeout.

### doctest — when the value matters

```pycon
>>> import numpy as np
>>> from doppler.source import LO
>>> np.round(LO(0.25).steps(4), 3)   # fs/4 tone: 1, j, -1, -j
array([ 1.+0.j, -0.+1.j, -1.-0.j,  0.-1.j], dtype=complex64)
```

Standard `# doctest:` directives (`+SKIP`, `+ELLIPSIS`, …) are honored — round
or use `+ELLIPSIS` for values with floating-point noise.

### include — zero drift

The strongest guarantee: show code that is *already* tested. The example
scripts in `src/doppler/examples/*.py` run in CI (`make test-examples-python`).
Mark a **self-contained** region (imports included) in the tested script:

```text title="src/doppler/examples/lo_demo.py"
# --8<-- [start:quarter_rate]
import numpy as np
from doppler.source import LO

lo = LO(0.25)  # a free-running quarter-rate tone
iq = lo.steps(8)  # 8 complex64 samples: 1, j, -1, -j, repeating
# --8<-- [end:quarter_rate]
```

Then pull it into the page with a single `--8<--` line inside a `python` fence.
The docs build inlines it — **and the gate resolves it too**, so the block is
really executed against the same code CI already tests. This very block is live:

```python
--8<-- "src/doppler/examples/lo_demo.py:quarter_rate"
```

The shown code *is* the tested code, so it cannot drift. Prefer this for new
gallery pages and any excerpt whose correctness (not just runnability) matters.
Note the region must run standalone — the gate executes exactly the marked
lines, so include the imports.

### skip — the last resort

For blocks that cannot run headless — a blocking network `recv()`, a hardware
source, a two-terminal demo, an intentionally-wrong example. Put the marker on
the line immediately before the fence. The reason is **mandatory** (a bare
marker fails the gate), so every exclusion is reviewed in the diff:

```text
<!-- docs-snippet: skip=blocking NATS recv, needs a broker; round-trip covered by stream tests -->
```

A block can also assert it raises: `<!-- docs-snippet: raises=ValueError -->`.

## The burn-down backlog

Pages not yet brought under the gate are listed in
`docs/.doc-snippet-ignore` — a **temporary** backlog that shrinks to empty, the
same idiom as `docs/api/.api-coverage-ignore` and `scripts/.serializable-ignore`.
A **new** page is never added here; it is gated on arrival. Each run prints
`doc-snippet backlog: N page(s) not yet gated`. To retire a page, make its
fences pass (or `skip=`-mark them with reasons) and delete its line.

## Building the docs locally (gotchas)

The docs toolchain has a few sharp edges — all now guarded, recorded here so
they are not rediscovered the hard way:

- **`docs/api.md` is generated by just-makeit**, not hand-written — don't delete
    it (the manifest-drift gate will fail).
- **`make docs` / `docs-serve` need the `docs` dependency group** (it holds
    zensical + the material theme). The Makefile targets pass `--group docs`; a
    bare `uv run zensical` in a dev-only venv renders themeless (no left nav).
- **A stale local `zensical.toml` shadows `mkdocs.yml`** (zensical prefers it),
    silently truncating the nav. The `docs`/`docs-serve` targets `rm -f zensical.toml` first.
