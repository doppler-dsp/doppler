# API Reference

## C API

The C layer is the source of truth — every algorithm is implemented here once.

- [**C API Reference**](../c-api/index.md) — file list, data structures, function index

## Python API

The Python modules are thin CPython extensions over the C ABI — no reimplementation.

| Module                                                     | Page                                                  |
| ---------------------------------------------------------- | ----------------------------------------------------- |
| `doppler.spectral` — FFT, correlation, spectral estimation | [Python: FFT & Spectral](python-fft.md)               |
| `doppler.spectral` — correlation & detection               | [Python: Correlation & Detection](python-spectral.md) |
| `doppler.spectral` — spectrum analyser                     | [Python: Spectrum Analyzer](python-analyzer.md)       |
| `doppler.detection` — detection statistics                 | [Python: Detection Statistics](python-detection.md)   |
| `doppler.source` — NCO, LO, AWGN                           | [Python: Source (NCO / LO / AWGN)](python-nco.md)     |
| `doppler.wfm` — waveform generator                         | [Python: Waveform Generator](python-wfmgen.md)        |
| `doppler.filter` — FIR, halfband                           | [Python: FIR Filter](python-filter.md)                |
| `doppler.ddc` — down-converter                             | [Python: DDC](python-ddc.md)                          |
| `doppler.resample` — polyphase resampler                   | [Python: Resample](python-resample.md)                |
| `doppler.agc` — automatic gain control                     | [Python: AGC](python-agc.md)                          |
| `doppler.streaming` — ZMQ streaming                        | [Python: Streaming](python-streaming.md)              |
| `doppler.buffer` — ring buffers                            | [Python: Buffer](python-buffer.md)                    |
| `doppler.delay` — delay line                               | [Python: Delay](python-delay.md)                      |
| `doppler.accumulator` — accumulators                       | [Python: Accumulator](python-accumulator.md)          |
| `doppler.cvt` — type converters & ADC                      | [Python: Type Converters](python-cvt.md)              |
| `doppler.arith` — saturating fixed-point ops               | [Python: Fixed-Point Arithmetic](python-arith.md)     |
| `doppler.measure` — tone/NPR/IMD metrics                   | [Python: Measurement Suite](python-measure.md)        |
| Polyphase filter bank                                      | [Polyphase → Resample](python-polyphase.md)           |
| `doppler.util` — shared numeric helpers                    | [Python: Utilities](python-util.md)                   |
