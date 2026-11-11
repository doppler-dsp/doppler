- **Three struct members whose type is an anonymous struct or union are
    documented on the member, not the type**:
    `dp_mpsk_receiver_state_t::fe`, `dp_RateConverter_state_t::agc_tlm_req`
    and the stream message's `u`. Each comment sat before `union {` or
    `struct {`, which Doxygen 1.18 attaches to the anonymous type and
    then reports the member undocumented; 1.9.8 did not notice (#3003).
