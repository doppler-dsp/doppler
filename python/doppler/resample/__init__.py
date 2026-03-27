"""doppler.resample — continuously-variable polyphase resampler.

The reference Python implementation lives in
:mod:`doppler.resample.reference`.  It uses the same C building blocks
(NCO, delay line, accumulator, polyphase bank) as the upcoming C
implementation and serves as the executable specification.

Resampler architecture
----------------------
::

    input ─► delay line ─► MAC ─► output
                  ▲          ▲
                  │          │
                NCO    polyphase bank[phase]
                  │
              overflow ─► push next input (interpolation)
                       └► emit output    (decimation)

NCO frequency
-------------
In both modes the NCO normalised frequency is the ratio < 1:

* **Interpolation** (``fs_out > fs_in``):
  ``nco_freq = fs_in / fs_out``
* **Decimation**    (``fs_out < fs_in``):
  ``nco_freq = fs_out / fs_in``

Polyphase branch selection:
``phase_idx = nco_phase >> (32 - log2(num_phases))``
"""
