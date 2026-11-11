- **The CLI's block API is public.** `doppler.cli` exports `Block`,
    `BlockConfig` and `register`, the classes a pipeline stage is written
    with, so they have entries in the generated Python reference and a
    tested example, `python -m doppler.examples.custom_block_demo`. The [CLI
    & Pipelines](docs/cli/index.md) page now shows that example's code
    (#3005).
