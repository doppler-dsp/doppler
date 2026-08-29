- **A frame stage's `kind` is now an open `uint32_t`, so a caller can add a
    stage without editing doppler's header.** It was a closed
    `wfm_stage_kind_t`, which made "a mission that is not CCSDS" a pull
    request against `wfm/wfm_frame.h` rather than a configuration — the
    opposite of what the description exists for. Kinds from
    `WFM_STAGE_USER` (0x1000) up are reserved for callers and doppler will
    never allocate there; the kernel arrives through `wfm_frame_ops_t` as
    before, and an unrecognised kind is still a refusal, never a silent skip.
    Source-compatible: every `WFM_STAGE_*` constant keeps its value.
