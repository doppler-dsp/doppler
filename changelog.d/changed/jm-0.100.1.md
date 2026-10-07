- **just-makeit pin 0.100.0 → 0.100.1.** Scaffold-side fixes only (a
    component with its own `reset()` passes its generated tests, gh-1882;
    `const char *` refused as a `step()` type, gh-1884). `jm apply`
    re-rendered nothing: no generated file changed.
