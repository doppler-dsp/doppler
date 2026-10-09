# API Reference

## C API

The C layer is the source of truth — every algorithm is implemented here once.

- [**C API Reference**](../c-api/index.md) — file list, data structures, function index

## Python API

The Python modules are thin CPython extensions over the C ABI — no reimplementation.

| Module                                                             | Page                                                      |
| ------------------------------------------------------------------ | --------------------------------------------------------- |
| `doppler.spectral` — FFT, correlation, spectral estimation         | [Python: FFT & Spectral](python-fft.md)                   |
| `doppler.spectral` — correlation & detection                       | [Python: Correlation & Detection](python-spectral.md)     |
| `doppler.analyzer` — spectrum analyser                             | [Python: Spectrum Analyzer](python-analyzer.md)           |
| `doppler.detection` — detection statistics                         | [Python: Detection Statistics](python-detection.md)       |
| `doppler.source` — NCO, LO, AWGN                                   | [Python: Source (NCO / LO / AWGN)](python-nco.md)         |
| `doppler.wfm` — waveform generator                                 | [Python: Waveform Generator](python-wfmgen.md)            |
| `doppler.wfm` — capture I/O (`Reader` / `Writer`)                  | [Python: Capture I/O](python-wfm-io.md)                   |
| `doppler.impairment` — propagation impairments                     | [Python: Impairment](python-impairment.md)                |
| `doppler.filter` — FIR, halfband                                   | [Python: FIR Filter](python-filter.md)                    |
| `doppler.ddc` — down-converter                                     | [Python: DDC](python-ddc.md)                              |
| `doppler.resample` — polyphase resampler                           | [Python: Resample](python-resample.md)                    |
| `doppler.agc` — automatic gain control                             | [Python: AGC](python-agc.md)                              |
| `doppler.stream` — NATS streaming                                  | [Python: Streaming](python-streaming.md)                  |
| `doppler.buffer` — ring buffers                                    | [Python: Buffer](python-buffer.md)                        |
| `doppler.telemetry` — scalar telemetry taps                        | [Python: Telemetry](python-telemetry.md)                  |
| `doppler.delay` — delay line                                       | [Python: Delay](python-delay.md)                          |
| `doppler.accumulator` — accumulators                               | [Python: Accumulator](python-accumulator.md)              |
| `doppler.cvt` — type converters & ADC                              | [Python: Type Converters](python-cvt.md)                  |
| `doppler.arith` — saturating fixed-point ops                       | [Python: Fixed-Point Arithmetic](python-arith.md)         |
| `doppler.measure` — tone/NPR/IMD metrics                           | [Python: Measurement Suite](python-measure.md)            |
| Polyphase filter bank                                              | [Polyphase → Resample](python-polyphase.md)               |
| `doppler.util` — shared numeric helpers                            | [Python: Utilities](python-util.md)                       |
| `doppler.ccsds` — CCSDS 131.0-B literals                           | [Python: CCSDS Literals](python-ccsds.md)                 |
| `doppler.acquire` — carrier, continuous and burst acquisition      | [Python: Acquire (CarrierAcquisition)](python-acquire.md) |
| `doppler.dsss` — DSSS despreaders, demodulator and receivers       | [Python: DSSS](python-dsss.md)                            |
| `doppler.mpsk` — M-PSK map, demap and soft demap                   | [Python: M-PSK Constellation](python-mpsk.md)             |
| `doppler.track` — second-order loop filter                         | [Python: Loop Filter (track)](python-track.md)            |
| `doppler.snr` — SNR / Es-N0 estimators                             | [Python: SNR](python-snr.md)                              |
| `doppler.coding` — channel codes (convolutional, RS, interleaving) | [Python: Channel Coding](python-coding.md)                |
| `doppler.ber` — bit/frame error-rate measurement                   | [Python: Error-Rate Measurement](python-ber.md)           |
| `doppler.interp` — interpolated table lookup                       | [Python: Interp (InterpolatedTable)](python-interp.md)    |
| `doppler.interrupt` — stopping a blocking run                      | [Python: Interrupt](python-interrupt.md)                  |
