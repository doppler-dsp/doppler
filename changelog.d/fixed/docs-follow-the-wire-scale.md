- **Four docs still described the old wire quantiser** after #1117 changed it —
    `types.md`, `design/capture-files.md`, `design/wfmgen-composition.md` and
    `guide/wfmgen/waveforms.md` gave full scale as `2^(N-1)-1` and one of them
    documented "truncating toward zero (a plain cast, not round-to-nearest)" as
    the contract. They are prose and tables, so no fence gate could catch them.
