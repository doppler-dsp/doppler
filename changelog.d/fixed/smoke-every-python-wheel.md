- **The post-release PyPI smoke installs every supported Python's wheel on
    every published platform, macOS arm64 included, and never the sdist.**
    It ran cp312 alone, so v0.55.0–v0.61.0's uninstallable cp313/cp314
    win_amd64 wheels went unseen (#1817). Its install now retries itself
    while PyPI's index catches up with the publish (#1394).
