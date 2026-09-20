- **A hand-written `_ext.c` is formatted by the gate, not by whoever runs
    `jm apply` next.** `make lint-clang-format` skipped every `_ext.c` as
    jm's; `buffer_ext.c` and `stream_ext.c` are not, so `jm apply` rewrote
    the former under the next person to apply. The set is derived from
    `no_generate` in the manifest.
