- **The named-args gate's helper discovery stops misreading three C
    shapes.** A quote inside a comment, a parameter feeding two enums and a
    variadic `...` could each hide a literal or make `--fix` write a wrong
    name (#2227).
