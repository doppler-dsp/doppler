- **`make jm-pin JM=x.y.z` — the just-makeit pin move, as a target.** The pin
    lives in `just-makeit.toml` (the SSOT), `pyproject.toml`, the downstream
    example's manifest and `uv.lock`. `gen_jm_pin.py --check` has gated that
    those AGREE since gh-693; nothing owned how they get there, so every bump
    improvised a `sed`, a direct script call and a bare `uv lock` — which is
    what the 0.63.3 → 0.65.0 bump did, in a repo whose rule is that the
    Makefile is the single driver.

    It **pre-flights the version against PyPI before touching anything**,
    because the failure mode is asymmetric: `docs-relink` runs under
    `uv run`, so once `pyproject.toml` names a version uv cannot resolve, the
    environment is broken and the target can no longer run to fix itself.
    Measured while writing it — `make jm-pin JM=9.9.9` left three files moved,
    a stale lock and no way back through make. Now an unreleased version
    changes nothing and says so:

    ```
    $ make jm-pin JM=0.66.0
    jm-pin: just-makeit 0.66.0 is not on PyPI — nothing changed.
      A merged fix is not a release.
    ```

    That is the question most often being asked at a bump, and the one a
    closed issue answers wrongly.
