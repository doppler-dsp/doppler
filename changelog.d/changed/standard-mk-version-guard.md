- **Re-vendored `standard.mk`: a bare exported `VERSION` can no longer arm a
    release target.** `VERSION` is exported by CI systems and shell profiles
    everywhere, and make imports the environment as make variables — so
    `VERSION=9.9.9 make bump-version`, with no argument typed, rewrote every
    manifest, and `release-branch` would have branched and bumped on it too.

    Fixed upstream rather than here
    ([just-buildit#29](https://github.com/just-buildit/just-buildit.github.io/pull/29)):
    the guard is generic to every repo on `standard.mk`, and a private copy in
    doppler's Makefile would have been the same two-implementations mistake
    that the version-site unification in this release exists to end.

    The fix is not a ban — that would cost the perfectly good
    `VERSION=x make …` habit to punish a name. It is that the NAME must carry
    evidence someone meant it: doppler declares `PROJECT = DOPPLER`, so
    `JUST_BUILDIT_DOPPLER_VERSION` is accepted from the environment while the
    bare name is refused for release targets and ignored everywhere else.
    Verified in this tree after re-vendoring — bare env refused, namespaced
    accepted, command line wins, and `make help` in a shell exporting
    `VERSION` still runs.
