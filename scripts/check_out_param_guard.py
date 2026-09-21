#!/usr/bin/env python3
"""Every binding that coerces an ``out=`` buffer must check it is contiguous.

An ``out=`` argument exists so the CALLER's array is filled. The bindings
validate it -- an ndarray, the output dtype, writable -- and then pass it
through ``PyArray_FROM_OTF(out_obj, ..., C_CONTIGUOUS | WRITEABLE)`` to get
a pointer for the C kernel. For an array that is not C-contiguous that call
does not fail: it makes a contiguous COPY. The kernel fills the copy, the
copy is dropped, and the caller's buffer is never written -- no exception,
and a fresh array comes back, so a caller reading only the return value
never finds out.

19 wrappers in 17 fragments did that (doppler#1440). Most are *sacred*
``_ext_<obj>.c`` files, which ``jm apply`` creates once and never
re-renders, so they kept the guard jm emitted the day they were written and
missed the contiguity half it gained later. A behavioural test covers the
types somebody remembered to list; this reads every binding, so a fragment
rendered before some FUTURE guard cannot regress the same way unnoticed.

The rule, per wrapper function: if it calls ``PyArray_FROM_OTF (out_obj``,
it must also test ``PyArray_IS_C_CONTIGUOUS`` -- refusing the buffer is the
only answer that does not involve a hidden copy.

Examples
--------
    $ python scripts/check_out_param_guard.py
    check_out_param_guard: OK -- 121 out= wrapper(s) in 58 file(s), all \
refuse a non-contiguous buffer
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
# A top-level C function: name, balanced-enough params, then a body that
# runs to the first column-0 closing brace. Bindings are clang-formatted
# GNU style, which puts both braces in column 0.
FUNC = re.compile(
    r"\n(\w+)\s*\((?:[^()]|\([^()]*\))*\)\s*\n\{(.*?)\n\}\n", re.S
)
COERCES = re.compile(r"PyArray_FROM_OTF\s*\(\s*out_obj")
GUARD = "PyArray_IS_C_CONTIGUOUS"


def main() -> int:
    bad, seen, files = [], 0, set()
    for path in sorted((ROOT / "native" / "src").rglob("*_ext*.c")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for m in FUNC.finditer(text):
            name, body = m.group(1), m.group(2)
            if not COERCES.search(body):
                continue
            seen += 1
            files.add(path)
            if GUARD not in body:
                bad.append(f"{path.relative_to(ROOT)}: {name}()")
    if bad:
        print("check_out_param_guard: FAIL")
        for b in bad:
            print(
                f"  - {b} coerces out= without refusing a non-contiguous "
                "buffer: a strided out= is silently copied and never "
                "written. Add `|| !PyArray_IS_C_CONTIGUOUS "
                "((PyArrayObject *)out_obj)` to its guard."
            )
        return 1
    if seen < 50:
        print(
            f"check_out_param_guard: FAIL -- only {seen} out= wrapper(s) "
            "found; the pattern stopped matching the bindings"
        )
        return 1
    print(
        f"check_out_param_guard: OK -- {seen} out= wrapper(s) in "
        f"{len(files)} file(s), all refuse a non-contiguous buffer"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
