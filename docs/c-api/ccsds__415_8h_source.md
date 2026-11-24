

# File ccsds\_415.h

[**File List**](files.md) **>** [**ccsds**](dir_c033bcb1c91e23f54b625bfe2cf0448c.md) **>** [**ccsds\_415.h**](ccsds__415_8h.md)

[Go to the documentation of this file](ccsds__415_8h.md)


```C++

#ifndef DP_CCSDS_415_H
#define DP_CCSDS_415_H

#ifdef __cplusplus
extern "C"
{
#endif

#define DP_CCSDS_415_SF 1023u
#define DP_CCSDS_415_FS 11e6
#define DP_CCSDS_415_SYM 3000.0
#define DP_CCSDS_415_FC_LO 2.2e9
#define DP_CCSDS_415_FC_HI 2.8e9
#define DP_CCSDS_415_SPAN_HZ 1500.0
#define DP_CCSDS_415_R_ENV_HZ_S 11.0
#define DP_CCSDS_415_FLOOR_ESN0_DB 4.0
#define DP_CCSDS_415_FRAME_SYM 1000u
#define DP_CCSDS_415_CODE_ONLY_SYM 275u
#define DP_CCSDS_415_LOCK_S 20.0

double dp_ccsds_415_chip_rate (double fc);

double dp_ccsds_415_cn0 (double esn0_db);

typedef struct
{
  double chip_rate;
  double ratio;
  double start_ppm;
  double rate_ppm_s;
} dp_ccsds_415_scene_t;

dp_ccsds_415_scene_t dp_ccsds_415_scene (double fc, double spc,
                                       double start_hz);

#ifdef __cplusplus
}
#endif

#endif /* DP_CCSDS_415_H */
```


