- **The `ccsds_tm` isolation gate now checks the rule the header states, not a
    broader one.** It scanned every component and ratcheted the four that
    include a `ccsds_tm` header toward zero — but `frame -> ccsds_tm ->   wfm_frame` is acyclic and deliberate, so three of those were the design
    working. It now reads `wfm_frame.{h,c}` alone and fails on an include, a
    call reached through a forward declaration, or reading nothing at all
    ([#1218](https://github.com/doppler-dsp/doppler/issues/1218)).
