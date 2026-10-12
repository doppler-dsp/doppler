- **The named-args gate's helper discovery stops misreading four C
    shapes.** A quote inside a comment, a parameter feeding two enums, a
    variadic `...` and a local that shadows a parameter could each hide a
    literal or make `--fix` write a wrong name (#2227).
