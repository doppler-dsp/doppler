- **The issue tracker marked issues "in review" that no PR closes.**
    `make issues` read every `#N` in an open PR's body as a close, so a PR
    that only mentioned an issue, or named one in another repository
    (`just-buildit/just-makeit#1307`), marked this repository's issue of the
    same number as under review. It now reads GitHub's own
    `closingIssuesReferences`, filtered to this repository.
