- **The named-args gate's helper discovery stops misreading four C
    shapes, and fails on a fifth it cannot read.** A quote inside a
    comment, a parameter feeding two enums, a variadic `...` and a local
    that shadows a parameter could each hide a literal or make `--fix`
    write a wrong name; a body whose braces never close is now a finding
    (#2227).
