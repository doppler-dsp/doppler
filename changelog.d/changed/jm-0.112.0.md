- **just-makeit pin 0.111.2 → 0.112.0: a string-enum parameter is documented
    by its choices, and a wrong capsule is a `TypeError`.** Every Python
    docstring for a string-valued parameter now lists what it takes
    (`One of "le", "be".`) instead of the C header's integer prose ("0 little,
    1 big", #2597); explanation that matters on the Python face moved into the
    manifest and is shown first. Passing the wrong object where a capsule is
    expected (`Writer.annotate(object())`, `AGC.set_telemetry(object(), …)`)
    now raises `TypeError` naming the parameter, not `AttributeError`
    (just-makeit #2355).
