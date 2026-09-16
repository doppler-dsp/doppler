- **The release bump is one command: `make release-pr VERSION=x.y.z`**. It
    branches off `origin/main`, bumps the five version sites, promotes the
    `changelog.d/` fragments, cuts the version section, regenerates the
    comparison links, re-checks the versions and the notes size, commits,
    pushes and opens the PR. `make changelog-assemble VERSION=x.y.z` does the
    section cut alone: it renames the `[Unreleased]` heading to the version
    and date and opens a fresh one, which was a hand step sitting directly
    after a command that had already edited the same file. It refuses to cut
    a version that already has a section.
