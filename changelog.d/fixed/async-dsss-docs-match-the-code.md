- **`AsyncDsssReceiver` and `AsyncDsssPool`'s docs say what the code
    does.** About forty statements the headers and their toml docs made
    were wrong or described the retired hand-off flavour: among them
    `code_rate` in chips per sample (chips per nominal chip), `locked` as a
    carrier flag (the symbol lock), `.rc` probes that never register, and
    `status()`/`symbols()` raising for a bad slot (they return -1 and
    empty). The header is now the one source of both Python faces.
