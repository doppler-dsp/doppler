- **Stock images are pulled through one helper that retries ECR Public's
    rate limit** (#1979). `scripts/stock-pull.sh` backs off on
    `toomanyrequests` and transient errors and fails at once on anything
    else. `make lint-stock-images` refuses a stock pull that bypasses it.
    The buildx container-driver builds are exempt pending #1982.
