

# File wfm\_names.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_names.h**](wfm__names_8h.md)

[Go to the documentation of this file](wfm__names_8h.md)


```C++
/*
 * wfm_names.h — SSOT name tables shared by the JSON scene serializer
 * (wfm_json.c) and the SigMF sidecar (wfm_writer.c).
 *
 * One definition: the [[enum]] manifest order IS the C int, append-only.
 * These existed as per-file copies until the wfm_writer one silently fell
 * behind (8 entries, no "dsss"; a `< 7` label guard that also dropped
 * "symbols") — the duplicated-table rot this header ends.
 */
#ifndef WFM_NAMES_H
#define WFM_NAMES_H

static const char *const TYPE_NAMES[]
    = { "tone",  "noise", "pn",      "bpsk", "qpsk",
        "chirp", "bits",  "symbols", "dsss" };
#define N_TYPES 9

static const char *const MODE_NAMES[] = { "auto", "fs", "ebno", "esno" };

#endif /* WFM_NAMES_H */
```


