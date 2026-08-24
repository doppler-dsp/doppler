- **just-makeit pin 0.63.3 → 0.67.1.** Taken for `process_global = true`
    (jm gh-1117 phase 2, shipped in 0.67.0), which is the mechanism
    [#976](https://github.com/doppler-dsp/doppler/issues/976) needs: jm links
    a component's OBJECT library statically into every extension module and
    CPython imports them `RTLD_LOCAL`, so the interrupt flag was one variable
    per `.so` and a stop requested through `doppler.interrupt` never reached a
    ring wait in `doppler.buffer`. Declaring the component makes jm generate
    the capsule rendezvous into every linking module's `PyInit_` — the owner
    publishes, everyone else adopts, import order irrelevant. jm's own
    `docs/shared-state.md` is written against this defect and names
    `dp_interrupt` as its worked example.

    0.67.1 is the pin rather than 0.67.0 because doppler has `no_generate`
    modules that link the primitive: 0.67.0 refused them outright and named an
    escape hatch that was never written, while 0.67.1 (gh-1128) reports an
    adopting `no_generate` module and emits the `<comp>_procglobal.h` whose
    three `#define`s let a hand-written binding join the rendezvous.

    The bump also brings gh-1114 — a `kind`-bearing module's keys are checked
    at last, where previously a handle or capsule module had **nothing**
    checked, so a key from the wrong face and an outright typo both read clean
    and both did nothing.
