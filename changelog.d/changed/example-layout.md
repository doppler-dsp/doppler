- **The examples now live where their language does.** `examples/c/` has
    moved to `native/examples/`, alongside the C it demonstrates, and the two
    stand-alone consumer projects — `examples/consumer/` and
    `examples/standalone/` — have moved to `example-projects/`, which says
    what they are: whole projects that build *against* an installed doppler,
    not demos that build *inside* it. That distinction was invisible while
    all three sat under one `examples/` directory, and it is the one thing a
    reader most needs to know before copying either.

    `src/doppler/examples/` is unchanged, and `examples/downstream-jm/` stays
    put — it is a jm project whose tree `jm status` walks, so it is neither a
    C example nor a consumer project.

    The built binaries moved with the source, from `build/examples/c/` to
    `build/native/examples/`. The output directory is now
    `${CMAKE_CURRENT_BINARY_DIR}` rather than a second written-out copy of
    the path, so it follows the source if this ever moves again instead of
    silently disagreeing with it.

    Nothing was renamed: `standalone` keeps its name, and all 20 files are
    pure renames in git's eyes, so history follows them.

    Sabotage-checked, and the check is worth recording because it found the
    gate's real edge: `check_doc_paths.py` catches a stale **backticked**
    path in a markdown page, and a **bare** one inside an example source,
    but it does not check paths under `build/` at all — `build/` is not a
    repo prefix, because it does not exist in a fresh checkout. So the
    ~20 `./build/native/examples/…` invocation lines this move rewrote are
    verified by no gate. Filed as doppler#967 rather than explained away.
