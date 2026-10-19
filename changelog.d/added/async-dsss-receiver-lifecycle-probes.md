- **`AsyncDsssReceiver` reports its own lifecycle as telemetry.**
    `set_telemetry()` now registers `<prefix>.state` (searching, refining,
    tracking, idle, lost as 0–4), `<prefix>.handover_hz` (the Doppler the
    chain was built from) and `<prefix>.refused` (hits refused as seeds),
    once per `steps()` call. When the search hit, how long the refine took
    and what it handed over now show as a time series beside the loops'
    probes. The header's probe list also says `<prefix>.rc` registers
    nothing here.
