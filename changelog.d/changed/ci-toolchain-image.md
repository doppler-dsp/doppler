- **Every Linux CI job runs inside a baked toolchain image instead of
    apt-installing one.** `deploy/docker/Dockerfile.ci` carries the dev and
    docs groups, and `.github/ci-images.env` pins it by digest.

    The provisioning step it replaces was ~112 MB of archives per job, ten
    jobs a run — and most of it was already on the runner under a different
    owner: cmake 3.31.6 and cargo 1.97.1 live in `/usr/local`, outside dpkg,
    so apt did not know they were there and fetched the distro copies anyway.
    That download was the entire exposure to mirror weather. On 2026-08-19 it
    stalled five runs of one PR, one job trickling 21m30s against a
    25-minute ceiling ([#885](https://github.com/doppler-dsp/doppler/issues/885)).

    **The image has no package list of its own.** It copies `bootstrap.toml`
    and runs the same two `jbx install-deps` commands `make install-deps` and
    `make install-docs-deps` run, so the image and a dev box provision from
    one file. A second list is exactly what `bootstrap.toml` exists to
    prevent, and it would rot in the way that is hardest to notice — the
    image would keep working while no longer being what a developer gets.

    **Two bases, and the pair is load-bearing.** `build-and-test` runs
    ubuntu-22.04 and ubuntu-24.04 on purpose — two glibcs — so one image for
    both legs would leave that matrix naming two environments while testing
    one. `BASE` is a build argument and both are published.

    **The glibc 2.28 floor is untouched.** It is still answered where it
    always was: `make glibc-gate` builds the tree in Debian 10 and runs
    `glibc-check` over the `.so` and every example binary, and release wheels
    are still built in `manylinux_2_28`. A modern base cannot answer a floor
    question, and this image does not try to.

    Two things fall out of running in a container. `make nats-up` shelled out
    to `docker run`, which does not exist inside a container job — and the
    nats:// tests **self-skip** when 4222 is unreachable, so that would have
    quietly dropped the whole nats path rather than failing. The image now
    carries `nats-server` and `scripts/start-nats.sh` prefers the binary,
    falling back to docker, so a dev box without docker gains those tests
    instead of skipping them. And the script no longer reports success on
    someone else's broker: it detects a listener it did not start and says so.

    Freshness is a question asked nightly, not an image consumed nightly.
    `ci-image.yml` rebuilds, compares the package fingerprint baked into the
    image, and publishes and opens a repin PR only when the content actually
    moved — so a run stays reproducible while drift still surfaces within a
    day. `make ci-image-check` fails offline when `bootstrap.toml` or the
    Dockerfile move without a repin, and refuses any `container:` naming a
    mutable tag.
