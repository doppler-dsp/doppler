- **Every Python benchmark now times the call it is named for.** The 17
    `bench_*.py` files that took the `benchmark` fixture and never called
    it, so collected nothing, are filled in. Each one asserts what its row
    claims, and `PY_HOLLOW_ALLOW` is empty. A test that requests the
    fixture without calling it is now an error under every pytest run
    (#1010).
