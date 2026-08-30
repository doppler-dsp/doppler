- **A page citing a `wfmgen` flag the parser rejects is caught now.** The gate
    asked one direction — every accepted flag is documented — so a renamed or
    deleted flag left the docs telling readers to type something the tool
    refuses. The reverse was left out because a prose scan cannot tell whose
    flag `--build` is; asking the question only of tokens inside a **wfmgen
    invocation** gives ownership for free. Measured first, as the issue asked:
    49 of 63 flags (78%) appear in such a fence, so the check sees most of the
    surface rather than a corner.
    [#1054](https://github.com/doppler-dsp/doppler/issues/1054).
