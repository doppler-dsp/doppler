- **A source no longer has `sync` or `crc`: the frame is a description, and a
    CRC exists only if it says so.** `Segment`/`Synth`/`Source` refuse
    `sync=` and `crc=` with any value, and a scene refuses the `"sync"` and
    `"crc"` keys, each naming `frame=` / `"frame"`. A frame that relied on the
    implicit CRC-16 trailer declares a CRC stage. `wfmgen --sync` and `--crc`
    stay and build the description for you: without `--crc` no CRC is sent
    (it was CRC-16), and `--record` stores a `"frame"` object; an old record
    with `"sync"`/`"crc"` is refused on replay
    ([#1617](https://github.com/doppler-dsp/doppler/issues/1617),
    [#1700](https://github.com/doppler-dsp/doppler/issues/1700)).
