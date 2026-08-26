- **`DsssBurstReceiver` exposes the two spans a caller must respect**
    ([#1011](https://github.com/doppler-dsp/doppler/issues/1011)).
    `refine_span` is the minimum burst spacing — detections closer than it
    are coalesced as one preamble, so tighter-packed bursts are lost — and
    `retain_span` is the history kept per anchor. Both were internal, so the
    only way to learn the spacing was to read the C, and the header's own doc
    for it was **2.4x low** (`2*reps*code_period` against the
    `(4*reps+4)*code_period` the code computes). Corrected and measured: at
    spacing exactly `refine_span` one burst of four is lost, and one sample
    more recovers it.
