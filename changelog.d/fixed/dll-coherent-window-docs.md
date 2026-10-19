- **`Dll`'s docs say its code loop sums coherently.** They said the
    `segments > 1` loop combined its partials non-coherently and was
    carrier-blind, with Costas downstream; it sums them over a one-period
    window, so a residual carrier of one cycle per period nulls it
    (`test_dll_core.c` §6d). `code_rate` now says it excludes the aid and
    reads up to one NCO step above the rate carried, and a carrier-present
    Python test that passed on a loop 4.8 chips off the code now checks the
    code phase.
