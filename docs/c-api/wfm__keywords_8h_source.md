

# File wfm\_keywords.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_keywords.h**](wfm__keywords_8h.md)

[Go to the documentation of this file](wfm__keywords_8h.md)


```C++

#ifndef DP_WFM_KEYWORDS_H
#define DP_WFM_KEYWORDS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define WFM_KW_MAX_TAG 255

  typedef struct
  {
    char     tag[WFM_KW_MAX_TAG + 1]; 
    char     type;      
    size_t   elem_size; 
    size_t   count;     
    uint8_t *value;     
  } wfm_keyword_t;

  size_t wfm_kw_elem_size (char type);

  int wfm_kw_check_standard(const char *tag, char type, const void *value,
                            size_t count);


  size_t wfm_kw_entry_size (size_t ltag, size_t vbytes);

  size_t wfm_kw_encode (uint8_t *out, size_t cap, const char *tag, char type,
                        const void *value, size_t count, int be);

  int wfm_kw_decode (const uint8_t *p, size_t avail, int be,
                     wfm_keyword_t *out, size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif /* DP_WFM_KEYWORDS_H */
```


