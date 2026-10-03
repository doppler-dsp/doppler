- **The shared `standard.mk` is pinned to the commit our vendored files match**
    (`STANDARD_URL` in the Makefile), so a canonical commit published after
    ours no longer turns every PR red in `standard-check`; the pin is lifted
    when the wiring it adds is adopted
    ([#1809](https://github.com/doppler-dsp/doppler/issues/1809)).
