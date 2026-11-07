- **Container fallbacks run as the caller under rootless podman too**
    (#2933). `-u uid:gid` cannot write a bind mount there, so `gen-c-api`'s
    doxygen fallback failed as a missing `index.xml`; one `CONTAINER_USER`
    now picks `--userns=keep-id` on podman.
