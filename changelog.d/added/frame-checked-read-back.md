- **`frame_checked` says how many checks actually ran.** `frame_valid` alone
    could not distinguish "the check failed" from "this frame carries no
    check", and an FER conflating them scores every unprotected frame as an
    error. Both are now read-backs and both are per-burst fields of
    `events()`.
