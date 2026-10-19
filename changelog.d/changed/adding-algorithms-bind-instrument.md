- **The algorithm lifecycle page gains its Bind phase and a telemetry
    pattern.** [Adding an algorithm](../../docs/dev/contributing/adding-algorithms.md)
    had no phase 5 and gave telemetry one line. Phase 5 now says what binding
    by declaration decides; phase 6 says what to decide about probes (pairs,
    units and cadence, a pinned list). Both use `AsyncDsssReceiver` as the
    worked example, and the telemetry design gains the composition pattern.
