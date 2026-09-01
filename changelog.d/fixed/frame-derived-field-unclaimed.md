- **A frame description whose derived field names no producing stage is now
    refused, with the reason** — it used to generate a *different waveform* in
    silence: 40 bits rather than 56, the unclaimed field dropped to zero
    length, the payload's own bits overwritten, exit 0 and nothing on stderr.
    Refused in `wfm_frame_desc_layout()`, where geometry is already decided,
    so one check covers the C API, the scene JSON, the CLI and Python
    ([#1155](https://github.com/doppler-dsp/doppler/issues/1155)).
- **A spec's refused frame reaches the CLI as a sentence.**
    `wfm_compose_from_json()` answers failure with a NULL, which can teach
    nothing; `wfm_compose_from_json_why()` carries the frame rule's message
    across, so `wfmgen --from-file` now names the missing `derived_by`
    instead of printing "could not build the waveform spec".
