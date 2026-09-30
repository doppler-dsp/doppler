- **A continuous dsss stream below one chip per symbol says so** (#1706).
    At the default `fs=1.0`, a `symbol_rate` in Hz left no chips per data
    symbol and was refused with a bare NULL. The CLI (naming
    `--symbol-rate`, `--fs` and `--sps`), a scene and C
    (`dp_wfm_source_error_fs`) now give the reason and the fix.
