# Doc examples — every snippet is tested

Every Python, C, **and shell** code example in the docs is checked in CI.
This is not a convention you have to remember — it is **enforced by
discovery**: a gate scans every page under `docs/` and runs its fences, so a
new page is covered the moment it exists. There is no opt-in list. The
Python gate lives in `src/doppler/tests/test_doc_snippets.py`, the C gate in
`src/doppler/tests/test_c_doc_snippets.py`, and the shell gate in
`src/doppler/tests/test_sh_doc_snippets.py` (all sharing include-resolution
and marker-parsing logic via `src/doppler/tests/_docs_snippet_common.py`);
run any of them locally with:

```sh
uv run pytest -m docs_snippets                            # all gates
uv run pytest -m docs_snippets test_c_doc_snippets.py      # C only
```

The shell gate covers the third fence class — documented CLI invocations
(```` ```sh ````/```` ```bash ````/```` ```console ````): every
`doppler ...`/`doppler-specan ...` line is parsed against the CLI's real
argparse parser (`build_parser()` — unknown flags, missing positionals, and
bad choices all fail), and a fence whose commands are all safe (`wfmgen`,
`cat`, …) with no live transport **executes end-to-end** under `bash -e` in
a throwaway per-page cwd. A ```` ```json title="scene.json" ```` fence is
materialized into that cwd first, so "here is the spec file, here is the
command that consumes it" runs exactly as shown. This class is where the
quickstart's `compose` CLI bugs (#458) and two wrong commands on the
architecture page lived — none of them could ever have worked. A fence
that depends on context outside shell (a file a Python fence wrote, an
unbounded `--realtime` stream) takes
`<!-- docs-snippet: no-exec=REASON -->`: still parse-validated, never
executed. Reasons are mandatory on every marker, in every gate.

The C gate needs the library built first (`make build`) — it compiles
each ```` ```c ```` fence against `build/libdoppler.a` (and
`build/libdoppler_stream.a`, when a snippet uses the `stream/stream.h` wire
layer) with `-std=gnu99 -Wall -Wextra -Werror`, then runs the binary and
requires exit code 0. `-Werror` means a warning is a failure here, same as
a compile error — this is what caught the homepage's own broken C
"Quick start" snippet (see below).

## Why

Doc snippets rot silently. The quickstart once showed `HalfbandDecimator()`;
the constructor later gained a required argument, the example started raising
`TypeError`, and nothing noticed for weeks. Prose examples are the first thing a
new user runs — they must always work. The homepage's own C "Quick start"
snippet had the same problem in a different language: missing
`#include <complex.h>`, undeclared arrays, and top-level function calls
outside `main()` — three separate compile errors — because nothing compiled
it. "Quick and easy" that silently doesn't compile is worse than not showing
C at all; the C gate below closes that hole the same way the Python one
already did.

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

The strongest guarantee: show code that is *already* tested. **Every**
script in `src/doppler/examples/*.py` runs in CI on arrival — the example
gate (`src/doppler/tests/test_examples.py`, `make test-examples-python`)
discovers them by glob, exactly like this gate discovers pages; the only
way out is a reasoned entry in `src/doppler/examples/.examples-skip`.
Examples are required to **validate themselves** (assert on a BER
threshold, a lock flag, a round-trip equality), so exit 0 means
"demonstrated and checked". Most gallery pages include their code from
these scripts, so page, script, and committed figure are one artifact.
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

### broker — conditional, not dead

`<!-- docs-snippet: broker=REASON -->` is for a **single-process** block
that needs only a live NATS broker: it runs whenever `127.0.0.1:4222` is
reachable — CI's python-tests job starts a JetStream broker, so it IS
executed in CI — and skips elsewhere. Same idiom as the stream suite and
the example gate's `broker:` registry entries. Use `skip=` instead when
the block needs a *peer process* (a two-terminal demo) or is an
illustrative fragment whose names come from prose.

## C fences

C has no REPL and no doctest notion, so a ```` ```c ```` fence has **three**
states instead of Python's four — no per-page shared namespace either, since
each fence is a fully independent compile-and-run:

| State       | How                                                | Proves                                                   | Use for                     |
| ----------- | -------------------------------------------------- | -------------------------------------------------------- | --------------------------- |
| **exec**    | plain ```` ```c ```` with its own `int main(void)` | compiles + runs, exit 0                                  | it *builds and runs*        |
| **include** | `--8<--` from a tested `examples/c/*.c`            | byte-identical to code `make test-examples` already runs | zero drift, by construction |
| **skip**    | `<!-- docs-snippet: skip=REASON -->`               | nothing (documents *why*)                                | —                           |

The same "runnable-first" bias applies: a fragment missing `main()` or an
`#include` is usually one edit from genuinely compiling — prefer fixing it
over skipping. Reach for `skip=` only for what truly can't run headless (a
blocking `recv()` waiting on a live broker/peer, or a struct-layout /
signature-only excerpt never meant to stand alone) — same bar as Python's.

## The burn-down backlog

Pages not yet brought under a gate are listed in an ignore file — a
**temporary** backlog that shrinks to empty, the same idiom as
`docs/api/.api-coverage-ignore` and `scripts/.serializable-ignore`:
`docs/.doc-snippet-ignore` for Python, `docs/.c-doc-snippet-ignore` for C.
A **new** page is never added to either; it is gated on arrival. Each run
prints `doc-snippet backlog: N page(s) not yet gated`. To retire a page, make
its fences pass (or `skip=`-mark them with reasons) and delete its line.

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
