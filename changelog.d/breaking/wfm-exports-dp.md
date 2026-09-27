- **The `wfm_*` C API is now `dp_wfm_*`**
    ([#1565](https://github.com/doppler-dsp/doppler/issues/1565)): 110
    exported functions and tables (`wfm_plan_render` → `dp_wfm_plan_render`,
    `wfm_reader_find_keyword` → `dp_wfm_reader_find_keyword`, …) and the type
    `wfm_compose_state_t`. The Python API is unchanged. `wfm_source_to_synth`
    stays bare for now (just-buildit/just-makeit#1694).
