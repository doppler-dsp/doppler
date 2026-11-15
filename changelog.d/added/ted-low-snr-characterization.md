- **Gardner against DTTL in `RateSync` at the specification's floor.** A
    characterization subject runs both on gapped rectangular BPSK at
    Es/N0 = 3 dB. The level fed to the loop matters more than the detector:
    Gardner slips 0 times in 1000 frames at half `RateSync`'s contracted
    level and over 700 at twice it. At the contracted level and the
    receiver's bandwidth both slip (58 to 85 times in 1000 frames), tie on
    SER between slips (0.3 to 0.6 dB of loss), and the DTTL's slips land in
    the idle run about twice as often, 34 against 15
    ([results](src/doppler/track/tests/characterization/README.md),
    [#751](https://github.com/doppler-dsp/doppler/issues/751),
    [#3079](https://github.com/doppler-dsp/doppler/issues/3079)).
