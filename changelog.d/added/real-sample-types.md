- **Real (scalar) sample types — `f32`, `f64`, `i32`, `i16`, `i8`.** doppler
    could read a real BLUE capture and not write one: the writer hardcoded
    format mode `'C'`. All four containers now write one component per sample
    (BLUE mode `'S'`, SigMF `rf32_le`), and the reader's hint can name a real
    type so a headerless real file is not read as interleaved I/Q.
    [#1032](https://github.com/doppler-dsp/doppler/issues/1032)
