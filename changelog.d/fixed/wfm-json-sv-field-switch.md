- **`wfm_json.c`'s row switches name `WFM_SV_FIELD`.** `row_get`/`row_set`
    stay exhaustive with no `default:`, so `-Wswitch` is quiet and the next
    new row kind is flagged rather than silently read as nothing (#1642).
