- **A `--record` capture now replays the run it recorded.** `seed_advance` was
    parsed onto the composer and never written back out, so a recorded run fed
    to `--from-file` replayed with the mode silently reset to `none` — the
    first loop matched and every loop after it did not. Nothing warned, on
    either stream. Same defect on the Python face, whose module docstring makes
    the same promise: `Composer.to_json()` dropped the key, so
    `from_json → to_json → from_json` produced a *different waveform*
    ([#978](https://github.com/doppler-dsp/doppler/issues/978)).

    The root cause was structural rather than a missed line: `wfm_spec_to_json`
    had **no parameter to write it from**. It takes one now, and the composer is
    the SSOT it reads — `wfm_compose_seed_advance()` is new, because
    `--from-file` sets the mode from the spec while the flag path sets it from
    `--seed-advance`, and a serialiser must not have to know which half
    supplied it. The Python face reaches the same one emitter through jm's
    already-declarative `to_json_trailing`, so there is still exactly one
    serialiser.

    The key is emitted **only when non-default**, exactly as `headroom` is
    omitted at 0 dB: the 1-source inline form's field order is frozen for
    byte-identity, and an always-present key would rewrite every capture ever
    recorded to say `none`. Verified: 32 of the 33 flag-matrix goldens are
    untouched, and the one that moved was pinning the defect.

    Two things made this easy to miss, and both are now written down where they
    are needed. It is observable **only on the repeat/continuous loop axis** —
    a segment's own `repeats: N` reseeds the AWGN unconditionally by design, so
    a `repeats`-based probe shows fresh noise under every mode and proves
    nothing. And the contract it broke was stated in three places
    (`wfm_compose.h`, `write_record()`, `compose.py`) and checked in none.

    Gated by `src/doppler/wfm/tests/test_cli_record_replays.py` — record and
    replay through the shipped binary, byte-comparing the samples, with
    companion cases pinning that the recorded run actually varied (so the
    comparison cannot pass vacuously) and that a default run's record is
    unchanged. Sabotage-proven: restoring the missing emit turns 6 of its 10
    cases red and leaves exactly the 4 default-path guards green.

    Still missing, and tracked on #978: a Python caller cannot *set*
    `seed_advance` — there is no `Composer` kwarg or attribute, only the JSON
    key. That needs a jm composer-kind feature; reading it back is what this
    fixes.
