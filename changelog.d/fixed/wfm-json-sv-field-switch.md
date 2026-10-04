- **`wfm_json.c`'s row switches name `WFM_SV_FIELD` and `WFM_SV_BESPOKE`.**
    `row_get`/`row_set` stay exhaustive with no `default:`, so `-Wswitch` is
    quiet and the next new row kind is flagged rather than silently read as
    nothing (#1642). The labels share the existing `WFM_SV_SYMBOLS` case, so
    they add no executable line the patch-coverage gate would count as
    unreached.
