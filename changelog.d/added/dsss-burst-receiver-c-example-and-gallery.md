- **`DsssBurstReceiver` gains a C example and a gallery page**, completing
    phase 9 under the rule above. The C example builds its capture from **one
    wfmgen segment** through `wfm_compose_create()` — the same engine the
    Python example's `Composer`/`Segment` and the `wfmgen` CLI use, so all
    three render the identical waveform and none of them tiles a preamble,
    spreads a frame, appends a CRC or draws noise. It demonstrates the four
    things the binding hides: the lifecycle, that the output buffer is the
    caller's and sized from `push_max_out()` on the *block*, and the two
    spans. [Gallery](https://doppler-dsp.github.io/doppler/gallery/dsss-burst-receiver/).
