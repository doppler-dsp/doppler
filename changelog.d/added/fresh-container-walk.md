- **`make docs-walk` walks every install path the docs name from a bare
    container.** Each stage (pip with the `[examples]` extra, the C tarball's
    three compile faces, a clone and `make build`, the .debs) runs the commands
    its own page gives; `WALK_MODE=candidate` is the pre-release dry run, and
    `release.yml` runs the published face after every release (#2857, #2879).
    Details: [the install paths the docs walk](https://doppler-dsp.github.io/doppler/dev/contributing/docs-conventions/#the-install-paths-the-docs-walk).
