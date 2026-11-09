- **Checked allocators: `dp_mallocn`, `dp_reallocn`, `dp_callocn` (NULL on
    overflow or OOM) and `dp_xmallocn`, `dp_xreallocn`, `dp_xcallocn`
    (abort)**, in `clib_common.h`. `make lint-alloc-helpers` now refuses a
    new inline product in any allocator's size argument
    ([#2963](https://github.com/doppler-dsp/doppler/pull/2963),
    [#2969](https://github.com/doppler-dsp/doppler/pull/2969)).
