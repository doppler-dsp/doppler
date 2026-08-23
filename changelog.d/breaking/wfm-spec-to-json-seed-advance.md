- **`wfm_spec_to_json()` takes a `seed_advance` argument.** The signature is now

    ```c
    char *wfm_spec_to_json(const wfm_segment_t *segs, size_t n_segs, int repeat,
                           int continuous, int seed_advance, double headroom);
    ```

    inserted before `headroom` because it belongs with `repeat`/`continuous` —
    all three are properties of the whole stream, while `headroom` is a writer
    gain. A caller with nothing to say passes `WFM_SEED_ADVANCE_NONE` (`0`) and
    gets byte-identical output to before.

    It is a parameter rather than a field read off `segs` for the same reason
    `repeat` is: a segment does not know how the stream loops. Adding it is what
    fixes [#978](https://github.com/doppler-dsp/doppler/issues/978) — the
    emitter previously had no way to write a key the parser read.
