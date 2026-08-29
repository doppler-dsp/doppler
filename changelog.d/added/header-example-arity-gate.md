- **A header's example must now call the functions in that header with the
    right number of arguments.** Nothing compiled a header `@code` block —
    `docs/**` fences are built `-Werror`, but a header's is only rendered by
    doxygen, published to `docs/c-api/**` and transplanted by jm into the
    `.pyi` — so examples drifted from their own signatures silently:
    `mpsk_receiver_create()`'s passed 16 arguments to 15 parameters and
    `ber_meter_score()`'s passes 11 to 5. Arity only, no compiler; Python
    doctest blocks are skipped on their `>>>`. The 11 that predate it are
    ratcheted and may only shrink.
    See [#1082](https://github.com/doppler-dsp/doppler/issues/1082).
