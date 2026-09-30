- **A `Synth`/`Segment` takes `frame=`.** A source's frame description now
    arrives from Python as a `FrameDesc` or a `Frame`, as it does from
    `wfmgen --frame` and a scene's `"frame"` key (just-makeit#1711, toward
    #1617). The source keeps its own copy. `frame=` is an input: read it
    back from `Composer.to_json()`. A `str` is refused, naming the object
    forms and the scene key. C gains `dp_wfm_frame_copy` and
    `dp_wfm_frame_to_json`.
