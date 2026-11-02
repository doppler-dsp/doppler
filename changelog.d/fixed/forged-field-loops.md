- **lockdet, farrow, adc, symsync, costas, carrier_nda and carrier_mpsk
    refuse a forged blob.** Each `set_state` now refuses what its create
    and setters refuse (a verdict, an order, a zero rng or avgs, a loop
    filter, an FLL assist), with the object unchanged. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
