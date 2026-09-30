- **A synth pulls its frames from a data source instead of cycling one**
    ([#1619](https://github.com/doppler-dsp/doppler/issues/1619)). Each
    frame is assembled over the next chunk when its first bit is due, so a
    CRC or an outer code covers that frame's own data. A paced source with
    nothing yet sends an idle frame of fill, and after the data ends the
    synth is silent rather than holding its last symbol. Not wired to a
    face yet.
