

# File wfm\_path.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_path.h**](wfm__path_8h.md)

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


