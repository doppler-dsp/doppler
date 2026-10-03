- **A source no longer has `sync` or `crc`: the frame is a description.**
    `Segment`/`Synth`/`Source` refuse `sync=` and `crc=` with any value
    (`crc="none"` included), and a scene refuses the `"sync"` and `"crc"`
    keys, each naming `frame=` / `"frame"`; the sync word is a field and the
    CRC a stage of the `FrameDesc`. `wfmgen --sync` and `--crc` stay and
    build that description for you (output unchanged), and `--record` now
    stores it as a `"frame"` object. An old `--record` carrying `"sync"` or
    `"crc"` is refused on replay, naming `"frame"`; re-record it, or move the
    two keys into a `"frame"` object
    ([#1617](https://github.com/doppler-dsp/doppler/issues/1617)).
