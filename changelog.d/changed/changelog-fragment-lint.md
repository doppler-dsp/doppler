- **A malformed changelog fragment fails `make lint` on its own PR.**
    `make lint-changelog-fragments` runs `changelog-assemble.py --lint`,
    which applies the assembler's own fragment rules (starts with `- `, no
    `### ` line) to every fragment in `changelog.d/`. They used to run only
    at the release cut, where two prose fragments stopped the v0.80.0
    `make release-pr` halfway (#2896).
