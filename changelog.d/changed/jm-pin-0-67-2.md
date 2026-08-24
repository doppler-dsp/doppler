- **just-makeit pin 0.67.1 → 0.67.2, retiring both `process_global`
    workarounds.** 0.67.0 shipped `process_global` with the adopt importing
    the PACKAGE (`doppler.interrupt`) while the publish landed the capsule on
    the extension module (`doppler.interrupt.interrupt`) — two different
    objects in jm's own `<pkg>/<mod>/<mod>.so` layout, so every adopting
    module raised `AttributeError` at import. doppler carried a
    `[module.interrupt.reexports]` entry to lift the capsule onto the package
    and make the two names meet. jm gh-1134 fixes the import path, so the
    entry is gone and with it the `docs/api/.api-coverage-ignore` line that
    excused `_jm_pg_dp_interrupt_guard` reaching a public `__all__`. The
    capsule is now private to the `.so`, which is what it always was.

    Also retired: the "keep the key, ignore the warning" note on `wfm_sink`'s
    `peak_dbfs`. 0.67.1's new key check warned that a getter field's `returns`
    "has no effect" while jm's own generator refuses an `expr` field without
    it (gh-333) — one manifest, two messages, each saying the opposite. jm
    gh-1137 adds `returns` to the vocabulary. Worth recording: 0.67.1 claimed
    the vocabularies had been validated against doppler's real manifest with
    zero findings, and jm's own 0.67.2 entry records that as wrong — the scan
    went one level deep and never descended into a row's inline-table arrays,
    where `returns` lives.

    The rest of the bump is handle-module polish doppler picks up for free: a
    handle method now carries a runtime `__doc__` (gh-1113 — `help()` was
    empty on every handle module while the `.pyi` beside it had the full
    prose), and a handle with no `create_args` no longer emits C that does not
    compile (gh-1131).

- **The generated `<comp>_procglobal.h` was stale and no gate was holding
    it.** gh-1134 moved the rendezvous import from the package to the
    extension module in the two places jm renders it — and reached only one of
    them on disk. `render_header` is called from `jm init` and `jm object`
    only, so `native/inc/dp_interrupt_guard/dp_interrupt_guard_procglobal.h`
    is written once at scaffold time and never again: `jm apply` does not
    rewrite it (clobber it to a single comment line and apply reports
    nothing), and `jm status --check` does not compare it. A `DO NOT EDIT`
    file sat outside every gate jm has.

    So after the bump the generated modules imported
    `doppler.interrupt.interrupt` while `DP_INTERRUPT_GUARD_PG_OWNER` still
    said `doppler.interrupt` — and `buffer` and `stream`, doppler's two
    `no_generate` modules, adopt through that macro. The package half-loaded:
    100 collection errors. Fixed by regenerating the header from jm's own
    renderer; filed upstream as
    [just-makeit#1140](https://github.com/just-buildit/just-makeit/issues/1140).

    `test_interrupt_is_process_wide.py` gains a third check, at the altitude
    the failure belongs at: the committed header must equal what THIS jm
    renders. The behavioural test did catch the breakage — that is how it was
    found — but as a hundred unrelated-looking import errors rather than as
    one stale file. Registration-free on both axes (components from
    `process_globals` over the merged manifest, expected text from jm's
    renderer), sabotage-proven against the exact one-line revert, and it also
    covers the shape doppler does not have today: a project whose adopters are
    all jm-generated, where a stale header breaks nothing until someone adds a
    hand-written binding.
