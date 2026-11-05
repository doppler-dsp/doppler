- **How to run a NATS broker, and an `[examples]` extra.** A new
    [Run a NATS broker](https://doppler-dsp.github.io/doppler/install/broker/)
    page is linked from every page that needs one (a lint gate holds it), and
    `pip install "doppler-dsp[examples]"` installs matplotlib and scipy, which
    an example that needs one now names instead of failing on the import. See
    [#2857](https://github.com/doppler-dsp/doppler/issues/2857).
