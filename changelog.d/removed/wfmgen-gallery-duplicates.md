- **Three duplicate wfmgen gallery pages are gone** — `plan`, `wfm-write`
    and `wfm-json`, with their scripts and figures. `Plan` is taught in
    [Scenes](docs/guide/wfmgen/scenes.md#prepare-once-sweep-many-plan) and
    exercised by the DSSS BER Monte Carlo; writing is `wfm-io`; the JSON
    round trip is an asserting fence in Scenes' `--record` section. Stale
    `rs_depth`, BER-recipe and frame-API prose fixed alongside
    ([#1682](https://github.com/doppler-dsp/doppler/issues/1682)).
