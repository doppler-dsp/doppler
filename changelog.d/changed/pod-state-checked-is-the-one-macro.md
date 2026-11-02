- **One checked POD serializer, and a shared forged-field test.**
    `DP_DEFINE_POD_STATE` and `_TLM` are now the checked forms with an
    always-true predicate, so the macro family has one body each, and
    `DP_STATE_FORGE_TEST` asserts a refusal leaves `get_state`
    byte-identical. See [#2142](https://github.com/doppler-dsp/doppler/issues/2142).
