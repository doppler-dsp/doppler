

Deprecated List



#### Module [**interrupt**](group__interrupt.md)  

These are the `dp_stream_*` spellings of a primitive that is not specific to streaming. It moved to `dp_interrupt.h` in the core library so a build with no NATS can use it; use `dp_interrupt()`, `dp_interrupted()`, `dp_resume()`, `dp_interrupt_on_signal()` and `dp_restore_signal()` instead. These forward verbatim and are removed once their callers migrate. See `docs/design/io-termination.md`.




    


