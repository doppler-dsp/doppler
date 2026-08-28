- **A hand-written `CHANGELOG.md` entry is now refused.** `changelog.d/` has
    asked for a fragment since twelve in-flight PRs all conflicted on the same
    file, but `changelog-check` accepted either — so three PRs in one stack
    still edited it directly and the conflict was hand-resolved twice. The
    gate counts `[Unreleased]` entries at the base and at HEAD;
    `changelog-assemble` still passes, detected by the fragments it consumes.
