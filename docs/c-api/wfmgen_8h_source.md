

# File wfmgen.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**wfm**](dir_d559aca39cc004340b6be1a6e35e20bd.md) **>** [**wfmgen.h**](wfmgen_8h.md)

[Go to the documentation of this file](wfmgen_8h.md)


```C++
/*
 * wfmgen.h — the wfmgen composer CLI, exposed as a plain callable.
 *
 * `doppler_wfmgen()` is the entire body of the `wfmgen` command-line tool with
 * the process `main()` stripped off: it takes an argv vector, runs the same
 * parse → compose → write/stream pipeline, and returns the shell exit code. The
 * standalone `wfmgen` binary is a one-line `main` shim over it.
 *
 * It is archived into libdoppler so a downstream that links libdoppler.a — or
 * loads libdoppler.so — can drive the full generator without shelling out. The
 * stream sink is statically linked, so there is no runtime client dependency. And
 * because it is the exact same code path, `doppler_wfmgen(argc, argv)` is
 * byte-identical to running `wfmgen …`.
 */
#ifndef DOPPLER_WFM_WFMGEN_H
#define DOPPLER_WFM_WFMGEN_H

#ifdef __cplusplus
extern "C" {
#endif

int doppler_wfmgen (int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* DOPPLER_WFM_WFMGEN_H */
```


