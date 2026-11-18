- **The docstring meter notices a class losing its authored Examples.** A
    class whose stub Examples is the hand-written `@code` block carries an `e`
    flag in `docs/.docstring-coverage-baseline` (66 of 120 classes today), and
    the per-symbol rule fails when it goes. Moving a binding's constructor
    to another C function had left `StateFilter` with the scaffold and nothing
    failed (#3162).
