# Vendored cJSON

Drop-in copy of [cJSON](https://github.com/DaveGamble/cJSON) — a single-file
ANSI-C JSON parser/printer (MIT).

- **Version:** 1.7.19
- **Source:** https://github.com/DaveGamble/cJSON (`cJSON.c`, `cJSON.h`)
- **License:** MIT — see `LICENSE`.

Not yet wired into the build — no consumer links it. When one does, compile
`vendor/cjson/cJSON.c` into that target (it is one self-contained translation
unit with no dependencies beyond the C standard library).

To update: re-copy `cJSON.c` / `cJSON.h` / `LICENSE` from the upstream tag.
