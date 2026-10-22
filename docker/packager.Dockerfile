# The packager image (HAS_PACKAGER_IMAGE). VENDORED from canonical: change it
# there, not here -- standard-check fails on any difference. Built by
# .github/workflows/ci-image.yml alongside the CI toolchain image, pinned by
# digest in .github/ci-images.env (CI_PACKAGER_*); why each pin exists is
# scripts/ci-image.py's docstring.
#
# A packager is a manylinux_2_28 image plus the packages a distro package
# build needs (rpm-build), baked in, so CI never reaches a distro mirror for
# them: the pinned digest is pulled and nothing runs a package manager. The
# base is manylinux_2_28 on purpose -- the glibc 2.28 floor is the claim the
# packages make, and a different base would move it.
#
# The packages come from the distro's LIVE mirrors, at the refresh that builds
# the image. There is no snapshot to pin, as ci.Dockerfile has with
# CI_APT_SNAPSHOT: dnf reads whatever the mirror serves that week. What holds
# the packages still is the fingerprint below (rpm -qa), which the workflow
# compares: a package that moved owes a repin, and a repin pins what was built.
#
# Every input is a build ARG with NO default, refused when empty, for the
# same reason as ci.Dockerfile: a default would be a second copy of the pin.
#
# The package list is not here. It is bootstrap.toml's groups
# (PACKAGER_GROUPS), installed by a PINNED just-bashit release's
# install-deps.sh, the same file `make install-deps` reads. install-deps runs
# as root with no sudo here, because it derives sudo from the caller.

ARG BASE
FROM ${BASE}

# The installer, PINNED: the same just-bashit release and sha256 check as
# ci.Dockerfile. The step is repeated here rather than shared: changing
# ci.Dockerfile would move every adopter's CI_IMAGE_SOURCE_HASH.
ARG JB_VERSION
ARG JB_SHA256
ARG PACKAGER_GROUPS
COPY bootstrap.toml /tmp/bootstrap.toml
WORKDIR /tmp
RUN set -eu; \
    [ -n "$JB_VERSION" ] && [ -n "$JB_SHA256" ] \
      || { echo "JB_VERSION and JB_SHA256 are required" >&2; exit 1; }; \
    [ -n "$PACKAGER_GROUPS" ] \
      || { echo "PACKAGER_GROUPS is required" >&2; exit 1; }; \
    curl -fsSL --retry 5 --retry-all-errors --retry-delay 2 \
      -o /tmp/jb.tar.gz \
      "https://github.com/just-buildit/just-bashit/releases/download/${JB_VERSION}/just-bashit.tar.gz"; \
    echo "${JB_SHA256}  /tmp/jb.tar.gz" | sha256sum -c -; \
    mkdir -p /tmp/jb && tar -xzf /tmp/jb.tar.gz -C /tmp/jb; \
    for g in $PACKAGER_GROUPS; do \
        bash /tmp/jb/src/just_bashit/install-deps.sh -g "$g" /tmp/bootstrap.toml; \
    done; \
    rm -rf /tmp/jb /tmp/jb.tar.gz /tmp/bootstrap.toml; \
    dnf clean all

# The fingerprint: every installed package and version, hashed. It changes
# only when the CONTENT does -- the question a repin answers. rpm, not dnf:
# a query must not reach a mirror.
RUN rpm -qa --qf '%{NAME}\t%{VERSION}-%{RELEASE}\n' | LC_ALL=C sort \
      > /etc/packager-image-packages \
 && sha256sum /etc/packager-image-packages | cut -d' ' -f1 \
      > /etc/packager-image-fingerprint \
 && echo "packager image: $(wc -l < /etc/packager-image-packages)" \
         "packages, fingerprint $(cat /etc/packager-image-fingerprint)"

WORKDIR /w
