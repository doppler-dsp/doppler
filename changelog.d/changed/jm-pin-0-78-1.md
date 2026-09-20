- **just-makeit pin 0.76.4 → 0.78.1.** Windows is clang-cl with no flag
    (MinGW retired, so doppler's `platforms` key is gone), every generated
    component gains a `test_<obj>_symbols.c` that fails at link time when a
    binding calls a function nothing defines, and `jm status --docs` lists
    members documented only by their name. A property now takes its doc
    from its OWN struct field, never a same-named field elsewhere — 35
    doppler properties were authored to match
    ([jm#1394](https://github.com/just-buildit/just-makeit/issues/1394)).
    doppler drove jm#1381, jm#1382, jm#1400 (a view's field docs) and the
    Windows DLL export fix.
