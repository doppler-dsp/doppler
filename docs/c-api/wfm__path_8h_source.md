

# File wfm\_path.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**wfm**](dir_3cdfcd43f00bf3b5a61213f071dd2284.md) **>** [**wfm\_path.h**](wfm__path_8h.md)

[Go to the documentation of this file](wfm__path_8h.md)


```C++

#ifndef WFM_PATH_H
#define WFM_PATH_H

#include <stdio.h>
#include <string.h>

static inline void
wfm_swap_ext (const char *path, const char *ext, char *out, size_t cap)
{
  const char *dot   = strrchr (path, '.');
  const char *slash = strrchr (path, '/');
  size_t      base  = (dot && (!slash || dot > slash)) ? (size_t)(dot - path)
                                                       : strlen (path);
  snprintf (out, cap, "%.*s%s", (int)base, path, ext);
}

static inline void
wfm_meta_path (const char *path, char *out, size_t cap)
{
  size_t n = strlen (path);
  if (n >= 11 && strcmp (path + n - 11, ".sigmf-data") == 0)
    wfm_swap_ext (path, ".sigmf-meta", out, cap);
  else
    snprintf (out, cap, "%s.sigmf-meta", path);
}

#endif /* WFM_PATH_H */
```


