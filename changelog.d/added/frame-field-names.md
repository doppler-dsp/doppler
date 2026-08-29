- **A frame field can carry a name**, and one lookup resolves it:
    `wfm_frame_field_index(d, "payload")`. Optional and behaviour-neutral — an
    unnamed description lays out identically, and every index-taking entry
    point is unchanged. It is the groundwork for addressing a stage's cover by
    name: `derived_by`, `first_field` and `n_fields` are all indices into the
    field array today, which is why a frame's every parameter has to be passed
    positionally and why `frame_create()` takes 38 arguments.
