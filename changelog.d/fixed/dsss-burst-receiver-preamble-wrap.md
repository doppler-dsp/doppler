- **`DsssBurstReceiver` refuses an `spc` whose sample counts wrap.** One
    code period (`acq_code_len * spc`), one burst
    (`(reps * acq_code_len + frame_syms * data_code_len) * spc`) and the
    preamble's byte size could each wrap, and construction then built a
    short or empty preamble and capture, or wrote past the preamble's
    buffer. Each is now checked before it sizes anything, and the receiver
    is refused with a `why` naming `spc`.
