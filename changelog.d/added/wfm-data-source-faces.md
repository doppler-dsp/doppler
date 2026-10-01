- **A frame's payload drawn from a data source, on every face**
    ([#1619](https://github.com/doppler-dsp/doppler/issues/1619)).
    `--data FIELD` / `"data"` / `Synth(data=bits)`, or
    `--data-from-file PATH` (`-` for stdin), is split into `--data-len`-bit
    frames, each with its own CRC, the last padded from `--fill`. A finite
    source sets the run's length; stdin ends it on a frame boundary, and
    under `--realtime` a pause sends idle frames. Code-only continuous DSSS
    is now `--code-only` (`"code_only"`); `--data none|prbs` is refused,
    naming it.
