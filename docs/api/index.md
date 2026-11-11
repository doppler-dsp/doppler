# API Reference

## C API

The C layer is the source of truth — every algorithm is implemented here once.

- [**C API Reference**](../c-api/index.md) — file list, data structures, function index

## Python reference

- [**Python API**][doppler] — every public class, function and constant, one page per module, generated from the package's `__all__`

## Python API

The Python modules are thin CPython extensions over the C ABI — no reimplementation.

| Module                                                             | Page                                                      |
| ------------------------------------------------------------------ | --------------------------------------------------------- |
| `doppler.spectral` — PSD, spectrogram & spectral helpers           | [Python: Spectral Analysis](python-spectral.md)           |
| `doppler.detection` — detection statistics                         | [Python: Detection Statistics](python-detection.md)       |
| `doppler.wfm` — waveform generator                                 | [Python: Waveform Generator](python-wfmgen.md)            |
| `doppler.wfm` — capture I/O (`Reader` / `Writer`)                  | [Python: Capture I/O](python-wfm-io.md)                   |
| `doppler.ddc` — down-converter                                     | [Python: DDC](python-ddc.md)                              |
| `doppler.resample` — polyphase resampler                           | [Python: Resample](python-resample.md)                    |
| `doppler.stream` — NATS streaming                                  | [Python: Streaming](python-streaming.md)                  |
| `doppler.telemetry` — scalar telemetry taps                        | [Python: Telemetry](python-telemetry.md)                  |
| `doppler.cvt` — type converters & ADC                              | [Python: Type Converters](python-cvt.md)                  |
| `doppler.measure` — tone/NPR/IMD metrics                           | [Python: Measurement Suite](python-measure.md)            |
| `doppler.util` — shared numeric helpers                            | [Python: Utilities](python-util.md)                       |
| `doppler.acquire` — carrier acquisition and burst capture          | [Python: Acquire (CarrierAcquisition)](python-acquire.md) |
| `doppler.dsss` — DSSS despreaders, demodulator and receivers       | [Python: DSSS](python-dsss.md)                            |
| `doppler.track` — second-order loop filter                         | [Python: Loop Filter (track)](python-track.md)            |
| `doppler.coding` — channel codes (convolutional, RS, interleaving) | [Python: Channel Coding](python-coding.md)                |
| `doppler.ber` — bit/frame error-rate measurement                   | [Python: Error-Rate Measurement](python-ber.md)           |
