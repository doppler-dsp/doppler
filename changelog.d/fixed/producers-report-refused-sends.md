- **A refused send is no longer silent.** wfmgen exits 1 when the stream
    refused any block, `dp_tlm_sink_lost()` counts dropped telemetry, and the
    C sends carry `DP_NODISCARD`, so dropping their status warns (#2011).
