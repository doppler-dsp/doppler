# Python Polyphase API

!!! warning "Removed in 0.3.0"
    `doppler.polyphase` has been removed.  The DPMFS polynomial-fit
    resampler (`ResamplerDpmfs`) has also been removed.

    **Migration:** use `doppler.resample.Resampler` for all rate
    conversion.  Its built-in 4096-phase × 19-tap Kaiser bank covers
    all practical use cases with no coefficient design step required.

    ```python
    from doppler.resample import Resampler

    r = Resampler(0.5)            # 2× decimation, 60 dB rejection
    y = r.execute(iq)
    ```

    For tighter specs, pass `rejection`, `passband`, and `stopband`:

    ```python
    r = Resampler(0.5, rejection=80.0, passband=0.35, stopband=0.45)
    ```
