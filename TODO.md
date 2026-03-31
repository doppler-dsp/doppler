# TODO

- Enforce data type consistency
- Rust sources should mirror C exactly
- ~~`simd.*` : We use SIMD everywhere. Doesn't need its own module. Let's create
  `util` and move there.~~ **DONE** (2026-03-28): `c/include/dp/util.h`,
  `c/src/util.c`
- ~~Remove any `dp_` prefixes from Python. Belongs in C and Rust only. Python has
  a built-in namespace via module dotting: `doppler.xyz`.~~ **DONE** (2026-03-28):
  All C extensions renamed to `_*` (e.g. `doppler._nco`, `doppler._stream`).
- Create a table mapping the API exposure


    | C                        | Python             | Rust              |
    | ------------------------ | ------------------ | ----------------- |
    | dp_acc_f32_t             | AccF32             | acc::AccF32       |
    | dp_acc_cf64_t            | AccCf64            | acc::AccCf64      |
    | dp_buffer_f32_t          | F32Buffer          | —                 |
    | dp_buffer_f64_t          | F64Buffer          | —                 |
    | dp_buffer_i16_t          | I16Buffer          | —                 |
    | dp_delay_cf64_t          | DelayCf64          | —                 |
    | dp_fft_*                 | doppler.fft.*      | fft::*            |
    | dp_fir_*                 | —                  | fir::Fir          |
    | dp_hbdecim_cf32_t        | HalfbandDecimator  | —                 |
    | dp_nco_t                 | Nco                | nco::Nco          |
    | dp_pub_t / dp_sub_t      | Publisher / Sub    | —                 |
    | dp_push_t / dp_pull_t    | Push / Pull        | —                 |
    | dp_req_t / dp_rep_t      | Requester/Replier  | —                 |
    | dp_resamp_cf32_t         | Resampler          | —                 |
    | dp_resamp_dpmfs_t        | ResamplerDpmfs     | —                 |
    | dp_c16_mul               | —                  | c16_mul           |

- ~~Modules live in sub-packages -- not the project root!~~ **DONE**
  (2026-03-30): All C extensions and `.pyi` stubs moved into subpackages.
  New: `nco/`, `buffer/`, `stream/`. Existing: `accumulator/`, `delay/`,
  `fft/`, `resample/` (combined `_resamp`, `_resamp_dpmfs`, `_hbdecim`).
  `fft/_fft.py` inlined into `fft/__init__.py` to resolve name conflict.

    ```python
    doppler/
        |__ subpkg_foo/ # YES!
        |       |__ __init__.py # Export baz via __all__
        |       |__ mod_baz.pyi
        |       |__ pure_mod_bar.py
        |
        |__ mod_bad.pyi # XXX NOT HERE!!
