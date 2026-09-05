- **`ccsds_tm` isolation is now gated, not just asserted.** `wfm/wfm_frame.h`
    says a component outside `ccsds_tm` must not include its headers, or the
    two form a cycle; nothing measured it, and four components did anyway. A
    ratchet in `make lint` fails on a new one **and** on an allowlist entry
    that no longer violates, so the list can only shrink rather than rotting
    into an exemption nobody rereads
    ([#853](https://github.com/doppler-dsp/doppler/issues/853)).
