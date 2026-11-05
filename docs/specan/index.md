# Spectrum Analyzer

Live spectrum analysis recorded with Doppler's C DSP pipeline:
NCO → Kaiser window → FFT (pocketfft) → dBm calibration.

No server. No JavaScript DSP. Pure C output, looped at 30 fps.

<div style="position:relative; width:100%; padding-bottom:62%; margin:1.5em 0; border:1px solid var(--md-default-fg-color--lightest); border-radius:4px; overflow:hidden;">
  <iframe
    src="demo.html"
    style="position:absolute; top:0; left:0; width:100%; height:100%; border:none;"
    title="Doppler spectrum analyzer demo">
  </iframe>
</div>

[Open full screen ↗](demo.html){ .md-button }

______________________________________________________________________

To run the live version against a real IQ source:

```sh
# Demo mode (synthetic signal)
doppler-specan --web

# File source
doppler-specan --source file --address capture.cf32 --fs 2.048e6 --web

# NATS stream from a doppler publisher (requires a running nats-server)
doppler-specan --source socket --address nats://127.0.0.1:4222/iq --web
```

A `nats://` endpoint needs a running [NATS broker](../install/broker.md); one command starts one.

Install:

```sh
pip install "doppler-dsp[specan-web]"
```

These commands run from a clone of the repository at the release you
installed; the `make` targets and the `./build/` binaries are not in the wheel.
Skip this if you already have one:

<!-- docs-snippet: no-exec=clones the repository -->

<!-- doc-version:start -->

```sh
git clone --branch v0.79.0 https://github.com/doppler-dsp/doppler && cd doppler
```

<!-- doc-version:end -->

Regenerate the recorded demo frames:

```sh
make record-demo
```
