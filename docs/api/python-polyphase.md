# Python Polyphase API

Kaiser window filter-bank design tools and DPMFS polynomial coefficient
fitting — used to generate the coefficient banks consumed by
`doppler.resample`.

Source:
[`python/dsp/doppler/polyphase/`](https://github.com/doppler-dsp/doppler/blob/main/python/dsp/doppler/polyphase/)

---

::: doppler.polyphase
    options:
      members:
        - kaiser_beta
        - kaiser_taps
        - kaiser_prototype
        - DPMFSCoeffs
        - fit_dpmfs
        - optimize_dpmfs
        - optimize_pbf
