- **just-makeit pin 0.82.2 → 0.83.0.** Three generated stubs (`buffer`,
    `dsss`, `telemetry`) used `Any` without importing it — a type checker
    saw an undefined name — and now import it; `wfm_reader.pyi` loses an
    import it never used. It also brings `jm adopt --check`, the read-only
    survey of which binding fragments could become fully generated
    ([#1446](https://github.com/doppler-dsp/doppler/issues/1446)), and lets
    the generated ring invariants test be linted again instead of excluded.
