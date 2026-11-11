# Gallery

Worked examples across every doppler subsystem — nearly all of them a
runnable script under [`src/doppler/examples/`](https://github.com/doppler-dsp/doppler/tree/v0.83.0/src/doppler/examples).
Most pages render a figure; **run `make gallery`
to regenerate every image**, or run a single script directly (each page lists
its command under *Reproduce* / *Run it*).

The scripts draw with matplotlib and some design filters with scipy, which are
not dependencies of the library. Install both once with
`pip install "doppler-dsp[examples]"`; a script that needs one and does not
find it says so and names that command.

A page whose subject is a C API instead carries a compiled-and-run `c` fence
and points at its demo under
[`native/examples/`](https://github.com/doppler-dsp/doppler/tree/v0.83.0/native/examples).

Looking for a runnable script that isn't a rendered walkthrough — LO, NCO,
FFT, ring buffers, the C API reference programs, or the NATS streaming demo?
Those live on the [Examples](../examples/index.md) page instead.

Every gallery page is listed in the navigation under **Gallery**.
