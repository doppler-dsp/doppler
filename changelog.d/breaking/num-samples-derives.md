- **`num_samples` defaults to 0, "derive it", and a count beside sources
    that set the length is refused on every face.** A finite data source
    (its frames) and a lone dsss burst (one burst) set a segment's on-time;
    `Segment(..., num_samples=1000)` beside one composed 24 samples without
    a word ([#1729](https://github.com/doppler-dsp/doppler/issues/1729)).
    One rule, `dp_wfm_scene_error`, now refuses it in Python, a scene and
    the CLI. C ABI: a zero-filled `wfm_segment_t` derives its on-time
    (1024 for a plain segment) instead of being empty, and `Plan.prepare`
    takes it; an empty on-time is a ranged `(0, 0)`.
