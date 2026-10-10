- **Every Python benchmark now times the call it is named for.** 17
    `bench_*.py` files were jm's scaffold: a fixture and no test at all, so
    they collected nothing. Each is filled in and asserts what its row
    claims, and `PY_HOLLOW_ALLOW` is empty. Separately, a test that requests
    the `benchmark` fixture and never calls it is now an error under every
    pytest run (#1010).
