- **`make install-deps` installs the NATS broker** (#2712). `nats-server` is
    now `bootstrap.toml`'s `nats` group, the release binary pinned by version
    and SHA-256, which the CI image installs too. A Linux dev box gets the
    broker CI tests against, so `make nats-up` no longer needs docker and the
    `nats://` tests run instead of skipping.
