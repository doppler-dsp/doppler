- **The `AsyncDsssReceiver`'s search lists one peak an emitter.** Its peak
    zone now covers every Doppler bin of every roll at one chip
    (`Correlator.set_peak_zone`, new), so a strong emitter's own lag no
    longer fills the list and starves a weaker one
    ([#3067](https://github.com/doppler-dsp/doppler/issues/3067), [#3060](https://github.com/doppler-dsp/doppler/issues/3060)).
