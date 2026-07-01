# Python Polyphase API

!!! warning "Removed in 0.3.0"

    `doppler.polyphase` has been removed. The DPMFS polynomial-fit
    resampler (`ResamplerDpmfs`) has also been removed.

    **Migration:** use `doppler.resample.Resampler` for all rate
    conversion. Its built-in 4096-phase × 19-tap Kaiser bank covers
    all practical use cases with no coefficient design step required.

    ```python
    import numpy as np
    from doppler.resample import Resampler

    iq = (np.random.randn(4096) + 1j * np.random.randn(4096)).astype(
        np.complex64
    )
    r = Resampler(0.5)            # 2× decimation, 60 dB rejection
    y = r.execute(iq)
    ```

    The built-in bank is fixed (≈60 dB rejection, 0.4/0.6 pass/stop) and has
    no per-instance spec knob in the Python API; design a custom bank in C
    via `Resampler_create_custom()` for tighter rejection.
