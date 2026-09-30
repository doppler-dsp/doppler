- **A refusal reason has one shape, and a gate holds it.** Every public
    refusal API names its cause through `const char **why`, the only shape
    jm binds. `docs/dev/contributing/error-convention.md` now states the
    rule, and `make lint-why-param` fails on any `why` / `*_why` parameter
    spelled another way (#1684).
