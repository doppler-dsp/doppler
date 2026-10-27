- **Every C header symbol is documented, and `make doxygen-check` fails on
    one that is not.** `EXTRACT_ALL = YES` made Doxygen count every symbol
    as documented, so about 1,130 symbols and ~900 missing `@param`/`@return`
    lines went unseen, 118 of them in headers with no `@file` block. Each is
    now written from its implementation, and the Python stubs pick them up:
    "Gain db." now gives a unit, range and role. A second Doxygen pass and
    `make lint-header-file-blocks` hold it; jm's own headers wait on
    just-makeit#2405, `.c` internals on #2639, and the code the comb found
    is #2640-#2654.
