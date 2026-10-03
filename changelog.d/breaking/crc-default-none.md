- **A source carries no CRC unless something asks for one.** `crc` defaults
    to `none` (it was `crc16`) on every face: `wfmgen` without `--crc`, a
    scene without `"crc"`, a Python source without `crc=`. A framed burst or
    data source that relied on the implicit trailer now says so: `--crc   crc16`, `"crc": "crc16"` or `crc="crc16"`, or a frame description with a
    CRC stage. Because `crc16` is now distinguishable from absent, a `crc`
    beside a carried `frame` is refused on every face, not just the CLI
    ([#1617](https://github.com/doppler-dsp/doppler/issues/1617),
    [#1700](https://github.com/doppler-dsp/doppler/issues/1700)).
