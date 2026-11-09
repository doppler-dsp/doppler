- **Two heap overruns in the coding group, and a `BurstDemod` preamble
    that could be left half-replaced.** A Viterbi `depth` whose
    `depth * nstate` wrapped to 0 got a `malloc (0)` that traceback then
    wrote past; `create_code` now refuses it. A `BurstDemod.set_preamble`
    whose `reps * est_segments` wrapped sized a short partials buffer that
    `form_partials` filled past its end; it is now refused. And
    `set_preamble` freed the old acquisition code before allocating the
    new one, so an out-of-memory there left an armed object with a NULL
    code that `demod` then read; it now acquires every new buffer first
    and changes nothing unless all of them are held.
