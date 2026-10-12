- **The named-args gate finds those helpers itself, replacing #2191's
    hand-kept table.** A `static` function, in the definition shapes the
    gate reads, that passes a parameter, bare, into a constructor's window
    or mode slot (or into another such helper) is held like the
    constructor. Nine of the table's ten are found; `rows_are_the_psd` and
    `oracle`'s mode only compare, so a literal mode at their calls is no
    longer refused (#2227) (#2197).
