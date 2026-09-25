- **A docs gate refuses a bare `[...]` in the generated C API pages.** mkdoxy
    sometimes emits a header's bracketed prose raw, and the strict site build
    rejects it as an unresolved link reference; the gate catches it at commit
    time instead of in CI's site build.
