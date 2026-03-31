# Python Resample API

Continuously-variable polyphase resampler — three implementations
all backed by the native C library, accepting and returning complex64
NumPy arrays with state preserved across calls.

Source:
[`python/doppler/resample/__init__.py`](https://github.com/doppler-dsp/doppler/blob/main/python/doppler/resample/__init__.py)

---

::: doppler.resample
    options:
      members:
        - Resampler
        - ResamplerDpmfs
        - HalfbandDecimator
