- **The named-args gate follows the helpers that forward a window or mode
    into a constructor.** The Spectrogram's C test, certification harness
    and bench forward them through helpers; their 16 literals, and 8 the
    gate cannot see, are names now. A definition or prototype is no longer a
    call (#2191). How the helpers are found is the entry below (#2197).
