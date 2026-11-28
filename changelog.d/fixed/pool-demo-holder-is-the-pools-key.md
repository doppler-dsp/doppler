- **The AsyncDsssPool demo names a holder by the pool's own key.** Both
    twins counted an emitter's holder within 1.5 chips, against the pool's
    one chip, so a search false alarm 1.5 chips from A read as A held twice
    and the demo failed at A's first epoch 91. The pool released it by
    design; the demo now passes at every first epoch from 85 to 95
    ([#3401](https://github.com/doppler-dsp/doppler/issues/3401)).
