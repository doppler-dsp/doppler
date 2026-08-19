- **A JSON scene with no `fs` rendered at 1 MHz, not at 1.0 — so a normalised
    frequency came out at DC.** `wfmgen --help` documents `--fs` as
    *"Sample rate (default 1.0; freq treated as normalised)"*, and the JSON
    reader defaulted the same field to **1e6**. A scene written to the
    documented contract —

    ```json
    {"version": 1, "segments": [{"type": "tone", "freq": 0.08}]}
    ```

    — was therefore read as 0.08 Hz against an unstated 1 MHz rate, which is
    a tone at DC. Nothing errored: the flag parser and the JSON reader are
    two faces of one generator, and they disagreed about a default.

    The reader now defaults `fs` to **1.0**, and the schema says so: `fs` is
    no longer in `required` for either segment form and carries
    `"default": 1.0` with the normalised-frequency contract spelled out. All
    three faces — flag, schema, reader — now agree.

    Found by `rate_converter_demo` failing its own frequency check with the
    tone **1245 bins** off, which is a self-validating example earning its
    keep; a silent DC tone has no other symptom. Gated by
    `test_json_fs_defaults_to_one_so_freq_is_normalised`, which asserts on
    the RENDERED waveform rather than the parsed struct and was proven by
    sabotage: restoring the 1e6 default gives `tone at 0.0, expected 0.08`.

    **Behaviour change for a scene that omitted `fs` and meant Hz.** Such a
    document was never schema-valid — `fs` was `required` — and every record
    `--record` writes states `fs` explicitly, so a round-tripped capture is
    unaffected. A hand-written scene relying on the old 1 MHz now needs
    `"fs": 1e6`, which it should have carried all along.
