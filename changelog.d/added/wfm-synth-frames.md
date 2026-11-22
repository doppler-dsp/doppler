- **`wfm_synth`: a type that plays finished frames from a callback.** A
    frame callback turns a frame of bits, drawn lazily from the bit refill,
    into samples played as they are, each frame once. The state blob (version
    seven) resumes it mid-frame and a restore never writes the renderer. C
    only for now: it is the base of the wfmgen `ofdm` type
    ([#3175](https://github.com/doppler-dsp/doppler/issues/3175)).
