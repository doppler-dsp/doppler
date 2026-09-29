- **`pn:LEN:1` with no POLY is refused where it is read.** A 1-bit register
    has no maximal-length polynomial, so it could never be built, but the
    parser accepted it and every face failed later, two with no reason
    given. It now fails at parse, naming the remedy (give POLY, or a wider
    REG). Found by exploring the parser (#853).
