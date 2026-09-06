- **`FrameDesc`'s builder verbs raise instead of returning `-1`.** `add_field`,
    `add_stage`, `add_hex`, `add_value`, `add_derived`, `add_stage_over` and
    `name_field` now raise `ValueError` naming the refusal; the successful
    return is still the new index. `field_index` keeps its `-1`, because a name
    that matches nothing is an answer rather than a refusal.
    [#1222](https://github.com/doppler-dsp/doppler/issues/1222).
