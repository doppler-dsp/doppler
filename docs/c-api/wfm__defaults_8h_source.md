

# File wfm\_defaults.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfm\_defaults.h**](wfm__defaults_8h.md)

[Go to the documentation of this file](wfm__defaults_8h.md)


```C++

#ifndef WFM_DEFAULTS_H
#define WFM_DEFAULTS_H

#define WFM_SOURCE_DEFAULTS \
  { \
    .snr = 100.0, \
    .sps = 1, \
    .pn_length = 15, \
    .modulation = 1 /* bitmod: bpsk */, \
    .rrc_beta = 0.35, \
    .rrc_span = 8, \
    .acq_reps = 1 \
  }

#define WFM_SEGMENT_DEFAULTS \
  { \
    .fs = 1.0, \
    .repeats = 1 \
  }

#endif /* WFM_DEFAULTS_H */
```


