- **A `wfm_source_t` can carry the frame a caller built.** The new optional
    `frame` field hands `wfm_frame_desc_t` straight to wfmgen, so a caller can
    name their own fields, spans and stage kinds instead of being limited to
    the thirteen flat framing and coding fields — which stay, as sugar that
    builds the same description. Kernels stay in C: a description names a
    stage's kind, and `wfm_frame_ops_t` supplies the code that runs it.
