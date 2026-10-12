- **The named-args gate follows a test's helpers, file by file.** Fourteen
    helpers in the Spectrogram's C test and certification harness forward a
    window or mode into a gated slot; their 16 literals are names now, and
    a definition or prototype no longer counts as a call (#2191).
