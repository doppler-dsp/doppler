- **The `AsyncDsssReceiver` specification demo runs the CCSDS 415.1-B-1
    forward link, configured for the Doppler rate the receiver meets.**
    `async_dsss_receiver_spec_demo` (Python and C) and its
    [gallery page](docs/gallery/async-dsss-receiver-spec.md) show Gold-1023
    at 3,000 sym/s and 11 MSa/s, at 2.2 and 2.8 GHz, at Es/N0 10 dB, through
    an 11 Hz/s Doppler ramp. That is the residual the link's ephemeris
    pre-compensation leaves (the design page's §1.8); the 400 Hz/s raw pass
    rate selects which passes are in scope and is not a rate the receiver
    sees, so the receiver is told 11 Hz/s as its `doppler_rate`, and its
    search runs as deep as the code-only window allows. It demonstrates the
    receiver; floor performance is the validators'.
