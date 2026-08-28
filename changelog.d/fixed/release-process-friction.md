- **`make tag-release` refuses to tag with `changelog.d/` fragments still
    unassembled, and `changelog-assemble` stages its own promotion.**
    `changelog-assembled-check` existed but ran nowhere — named in the
    .PHONY/help list and in no gate, no CI job — the third instance of that
    shape in this repo. Its home is the irreversible step, not `lint`, since
    a feature branch legitimately carries fragments. Assembling also used to
    leave `make lint` failing on deleted-but-tracked paths; it now stages
    itself. `docs/dev/release.md` gains the assemble step, which it had never
    mentioned.
