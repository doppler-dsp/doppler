- **just-makeit pin 0.135.1 → 0.137.0.** A `Segment`'s `sources` list
    refuses a foreign element with a `TypeError`; the stubs type
    `EventLog.finalize`'s and the SigMF sinks' and writers' `sample_type`
    and `endian` as `Literal`; `dp_interrupt_guard_procglobal.h` declares
    its two functions with C linkage, so a C++ caller links them
    ([just-makeit#2754](https://github.com/just-buildit/just-makeit/issues/2754)).
