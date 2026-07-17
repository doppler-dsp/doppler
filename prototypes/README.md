# Prototypes

Working area for validating risky DSP redesigns in pure Python before they
touch C — see `~/.claude` memory `feedback_python_prototype_before_c.md`.
Nothing here is built, packaged, or imported by `src/doppler`; each
subfolder is a self-contained, throwaway-but-committed scratch space for
one investigation, kept around so the validation is reproducible instead
of living only in an ephemeral session scratchpad.

## `async_despreader/`

Investigates a long-run divergence bug in `Dll`'s `segments > 1` async
chunked-lookback path (`native/src/dll/dll_core.c`'s
`dll_steps_impl` segments-branch). See its `README.md` for status.
