- **Re-vendored `standard.mk`.** Upstream made `install-deps`'s recipe an
    overridable `INSTALL_DEPS_CMD` (default unchanged: bootstrap `jbx`, then
    `jbx install-deps`), so the repo that owns the installer script can run
    its source under development instead of the published copy. doppler sets
    nothing and behaves as before; the vendored copy being behind failed
    `standard-check` on every branch, which is why this is its own change.
