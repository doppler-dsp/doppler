- **A `dll_set_code_phase()` across the period wrap no longer emits or skips
    a period's partials**
    ([#1287](https://github.com/doppler-dsp/doppler/issues/1287)). A put
    backwards across the wrap left the segment bookkeeping past every
    segment's end — a period of garbage partials in four samples — and a put
    forwards skipped the wrap the epoch was waiting for, folding a second
    period into it. Measured as a symbol slip every few intervals in
    `async_dsss_receiver`'s cell mode. The put now records which way it
    crossed and the epoch follows it, on both correlation paths.
