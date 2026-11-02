- **just-makeit pin 0.114.0 → 0.114.2, and each module's manifest is its
    own file.** `make jm-migrate-fragments` moved every `[module.X]` table
    out of `just-makeit.toml` into `modules/<name>.toml`, beside
    `objects/`. 0.114.1 made the move lossless for values: no new
    `jm status` entry, and every package's `__all__` unchanged. 0.114.2
    made it lossless for comments: each table takes its own comments to
    its fragment, byte for byte (just-makeit gh-2437).
