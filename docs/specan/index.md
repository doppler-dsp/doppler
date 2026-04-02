# Spectrum Analyzer

Live spectrum analysis recorded with Doppler's C DSP pipeline:
NCO → Kaiser window → FFTW → dBm calibration.

No server. No JavaScript DSP. Pure C output, looped at 30 fps.

<div style="position:relative; width:100%; padding-bottom:62%; margin:1.5em 0; border:1px solid var(--md-default-fg-color--lightest); border-radius:4px; overflow:hidden;">
  <iframe
    src="demo.html"
    style="position:absolute; top:0; left:0; width:100%; height:100%; border:none;"
    title="Doppler spectrum analyzer demo">
  </iframe>
</div>

[Open full screen ↗](demo.html){ .md-button }

---

To run the live version against a real IQ source:

```sh
# Demo mode (synthetic signal)
doppler-specan --web

# File source
doppler-specan --source file --address capture.cf32 --fs 2.048e6 --web

# ZMQ stream from a doppler publisher
doppler-specan --source socket --address tcp://localhost:5555 --web
```

Install:

```sh
pip install doppler-specan[web]
```

Regenerate the recorded demo frames:

```sh
make record-demo
```
