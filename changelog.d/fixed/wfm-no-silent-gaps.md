- **A scene with a source the generator cannot build is refused, not
    rendered as silence.** `wfmgen --pn-length 65` used to write 0 bytes and
    exit 0; the composer now builds each source once when it is created, through
    the same builder the render uses, and refuses the scene (#1590).
