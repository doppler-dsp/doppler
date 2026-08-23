- **`dp_mean_power()` / `doppler.stream.mean_power()` — mean power of a
    complex block, normalised to full scale.** `mean(|x|**2)` with the
    integer formats divided by `dp_format_full_scale()` first, so the answer
    means the same thing whatever the wire carried and `10*log10()` of it is
    dBFS in every case. `dp_msg_mean_power()` is the same thing over a
    received frame, which lets a subscriber report power without branching
    on the format at all.

    It exists because the examples were each carrying their own copy: two in
    `examples/c/receiver.c` (one per wire type, plus a switch to choose), a
    third in `pipeline_demo.c`, and a numpy expression in the Python
    receiver — four implementations of one formula in a DSP library whose
    own rule is that a primitive is written once. All four now call it.

    Also new: `doppler.stream.format_name()` (the same `dp_sample_type_str()`
    the C face prints, so neither receiver needs a private code-to-name
    table) and `data_rep` in the `recv()` header dict.
