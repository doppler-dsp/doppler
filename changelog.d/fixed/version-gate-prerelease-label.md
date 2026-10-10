- **The hand-typed-version docs gate lets a pre-release label through.**
    `check_version_strings` matched doppler's version as a prefix, so a page
    recording the label a measurement ran under (`VERSION=0.66.0-a4`) read as
    the release version hand-typed and blocked `make release-pr`. A SemVer
    pre-release suffix (`-a4`, `-rc1`) now ends the match; a hyphenated word
    such as `0.66.0-based` is still refused.
