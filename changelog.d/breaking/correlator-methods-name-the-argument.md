- **Eleven correlator C functions take a trailing `const char **why`**:
    `dp_correlator_execute`, the five `execute_*` batches, both `bank_*`,
    `set_telemetry`, `configure_search_raw` and `set_max_peaks`. A refusal
    writes the sentence naming the refused argument, which Python raises;
    pass NULL to ignore it ([design §18](docs/design/correlator-python.md)).
