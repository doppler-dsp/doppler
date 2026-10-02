- **`FrameDesc.add_data(name, len)` describes a `data:LEN` field from
    Python** (C: `dp_frame_add_data`). It is the field the Field text
    `data:LEN` parses to, so a `Source(frame=)` holding it sends a
    multi-frame data source byte-identical to a scene's `"frame"` key or
    `wfmgen --frame`. The receive side (`build()`, `deframe()`, `check()`
    on such a description) is not built yet
    ([#1789](https://github.com/doppler-dsp/doppler/issues/1789)).
    ([#1786](https://github.com/doppler-dsp/doppler/issues/1786))
