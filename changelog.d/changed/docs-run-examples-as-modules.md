- **The docs run examples as `python -m doppler.examples.<name>`.** The 44 run
    lines that said `python src/doppler/examples/<name>.py` worked only in a
    clone; the installed wheel ships the same scripts, so every page now uses
    the module form, names the `[examples]` extra where a script needs it, and
    links the script behind a figure at the release. Lint refuses the old form.
    See [#2857](https://github.com/doppler-dsp/doppler/issues/2857).
