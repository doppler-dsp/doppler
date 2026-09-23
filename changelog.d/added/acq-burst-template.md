- **`acq_create_burst_template()`: burst acquisition of any repeated complex
    preamble** — a chirp, a Zadoff-Chu sequence, shaped PSK — not only a PN
    code. One chip is one sample, and what a code knows analytically (the peak
    zone, the delay straddle) comes from the template's own correlation, from
    one FFT at construction. Every derived value is checked against an
    independent reference: the analytic chip shape, a closed form for odd and
    even lengths, and injected delay and Doppler. Phase 2 of
    [#1470](https://github.com/doppler-dsp/doppler/issues/1470); C only, with
    the Python face in phase 3.
