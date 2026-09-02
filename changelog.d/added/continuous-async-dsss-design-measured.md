- **The continuous async-DSSS receiver has one design page, and its three
    load-bearing numbers are measured.** `docs/design/async-dsss-receiver.md`
    consolidates the spec, the despreader, the many-emitters use case and the
    searcher design; two new C harnesses in `native/validation/` measure the
    floor one emitter sets on the search surface (the fork moves from −24 to
    −13 dB) and when the tracking receiver can say an emitter is gone (both
    lock flags down for longer than the fade — code lock alone chatters), and
    operating-point rows on three benches price the searcher at 2.1× real
    time per core over ±50 kHz and one receiver at 0.44 of a core. Details
    in the page's §12.
