- **34 example docstrings told you to run a path that has not existed for
    releases.** Every Python example's `Usage:` block said
    `python examples/python/<name>.py` — 46 lines, in the block a reader
    copies first — and the Python examples moved under `src/doppler/` long
    before that. The invocation is now the bare script name: a filename is
    the file itself, while a path is a claim about where the tree keeps it,
    and the tree moves.

    Nothing looked, which is the actual defect. `check_doc_paths.py`
    validates every repo path named in prose, and its sources were
    `docs/**/*.md` and `native/inc/**/*.h` — an example's docstring is read
    as documentation but was checked as code. The examples are now in
    scope, and inside them a **bare** path counts as much as a backticked
    one, because a "Usage:" line is where they actually appear. That second
    rule is deliberately not applied to the markdown pages, whose prose
    says "under docs/design" far more loosely; a rule that fires on those
    is a rule that gets switched off.

    Sabotage-checked: putting one `examples/python/` line back fails the
    gate by name. It found a real straggler on arrival too —
    `examples/c/agc_demo.c` cited the same dead directory.
