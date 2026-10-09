- **`dp_fir_execute` / `FIR.execute` no longer depend on where a stream is
    split.** On AVX2, AVX-512 and NEON builds the last samples of every call
    were rounded differently from the rest (a rounded product then a rounded
    add, against a fused multiply-add), so the same input gave a different
    last bit depending on chunking, and a state hand-off was not bit-exact.
    **The last bit can differ from previous releases on those builds**;
    portable (SSE2) builds are unchanged. A call shorter than one vector
    group (under 8 complex samples on AVX-512) now costs more per sample,
    up to several times for a long filter (cost table in #1932; #1893).
