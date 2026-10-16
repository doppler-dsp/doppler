- **The Dll's taps run on integer phase words, about 3x faster in a held
    async DSSS receiver.** Each tap's code phase was a chip count in
    double folded by `fmod`; it is now the code NCO's own 32-bit word, so a
    held receiver at 5 Mcps costs 11.2 ns a sample, not 33.1. The despreader
    and `DsssReceiver` share the Dll and the gain
    ([#2397](https://github.com/doppler-dsp/doppler/issues/2397)).
