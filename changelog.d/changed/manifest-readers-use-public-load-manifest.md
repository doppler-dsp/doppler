- **Manifest readers go through jm's public `load_manifest`.** Six callers
    left the private `just_makeit._config`, and `check_wfm_enum_tables` now
    sees an `[[enum]]` declared in a fragment. `check_user_docs` refuses a
    manifest with no `record_name`. See
    [#2751](https://github.com/doppler-dsp/doppler/issues/2751).
