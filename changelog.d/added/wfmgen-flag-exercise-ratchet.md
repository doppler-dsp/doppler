- **The wfmgen flags nobody runs can no longer grow in number.** The gate has
    printed "53 of 67 flags exercised" as its second line since it was
    written, and nothing read it. That count is now a ratchet checked both
    ways: a documented-but-unrun flag fails unless it is waived, and a waiver
    that outlives its defect fails too. Closes
    [#1143](https://github.com/doppler-dsp/doppler/issues/1143); paying the
    debt down is [#1149](https://github.com/doppler-dsp/doppler/issues/1149).
