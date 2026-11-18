- **`wfm`: a documented default is compared with the object's actual one.**
    The engine's `fs` parameter now says what it does (default 1e6, so `freq`
    is in Hz against 1 MHz), and a test reads each `name : type, default V`
    line of the `Synth` and `Segment` docstrings and requires a bare object's
    attribute to equal it. `Synth.fs` is documented as 1.0 and runs at 1e6
    ([#3142](https://github.com/doppler-dsp/doppler/issues/3142)); that is a
    shrink-only exemption until jm's composer generator reads the default and
    its documentation from the object
    ([just-makeit#2678](https://github.com/just-buildit/just-makeit/issues/2678)).
