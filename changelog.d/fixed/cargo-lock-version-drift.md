- **`ffi/rust/Cargo.lock` stated doppler `0.46.0` against a `0.47.0` project** —
    one full release behind, through a release that shipped. `bump-version`
    re-synced uv's lockfile and had no cargo equivalent, and nothing reads the
    file, so cargo silently rewrote it on the next build. It is regenerated at
    bump time now, and `make lint` fails when it lags. Closes
    [#1316](https://github.com/doppler-dsp/doppler/issues/1316).
