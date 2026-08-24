- **just-makeit pin 0.67.1 → 0.67.3, retiring both `process_global`
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
    it — fixed upstream in 0.67.3 (gh-1140, doppler-filed).** gh-1134 moved
    the rendezvous import from the package to the extension module in the two
    places jm renders it, and reached only one of them on disk:
    `render_header` had exactly two callers, `jm init` and `jm object`, both
    scaffolding. So `native/inc/dp_interrupt_guard/dp_interrupt_guard_procglobal.h`
    was written once when the component was created and never again — `jm   apply` would not rewrite it and `jm status --check` did not compare it.

    That is the file a `no_generate` module reads the rendezvous names out of,
    which is what gh-1128 published it for. `buffer` and `stream` adopted
    through the stale macro and the package half-loaded: 100 collection
    errors, under a pin bump whose entire subject was that one line.

    0.67.3 reconciles the header from the manifest like any other glue, so a
    stale one is rewritten, a clobbered one restored, and `--check` fails on
    either. Verified here by clobbering it to a single comment line: `jm   apply` restores it byte-identically to the value this branch had briefly
    hand-patched, and `make drift-check` fails while it is wrong. The
    hand-patch is gone — the file is jm's again, and its `DO NOT EDIT` banner
    is true for the first time.

    The second half of the issue is why the first survived a release, and jm
    took the general fix rather than the specific one: its new gate clobbers
    **every** file in a scaffolded project and demands `status` notice, with a
    named exemption for the files whose content is the author's — so a file jm
    learns to generate is covered the day it is generated, with no list to
    update. It found gh-1141 on its first run.

    doppler's own check is retired with the workaround it guarded: with the
    header inside `jm status --check`, keeping a second assertion over jm's
    private `_procglobal` API would be two sources of truth for one claim and
    a standing hostage to an internal rename. What stays is the pair that
    tests something jm cannot see — that a stop in one module reaches a wait
    in another, and that every `.so` carrying the flag joined the rendezvous.

    One thing outlives it. Writing that check found
    `test_interrupt_is_process_wide.py` spelling the checkout as
    `_PKG.parents[1]`, which is right exactly once — from the source tree.
    `make coverage` runs the suite from a copy at `build-cov/pkg/doppler`, two
    levels deeper, so the header was unreachable and
    `test_every_module_carrying_the_flag_joins_the_rendezvous` had been
    **skipping under coverage** rather than failing, silently, since it was
    written. It now uses `doppler.tests._repo.repo_root()` — which exists for
    exactly this and whose own docstring is about exactly this failure — and
    the check runs in that job. Measured both ways from a relocated copy: the
    old spelling skips, the new one runs.
