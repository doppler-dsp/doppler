#!/usr/bin/env bash
# Start a NATS JetStream broker on 127.0.0.1:4222 and wait until it accepts
# connections. The stream suite's nats:// tests self-skip when the port is
# unreachable, so CI starts a broker to actually exercise them (a GitHub
# service container can't pass the `-js` JetStream flag, so it runs explicitly).
# One definition, shared by every ci.yml job that needs the broker.
set -euo pipefail

docker run -d --name nats -p 4222:4222 nats:2.10 -js
for _ in $(seq 1 50); do
  (exec 3<>/dev/tcp/127.0.0.1/4222) 2>/dev/null && break || sleep 0.2
done
echo "NATS JetStream broker ready on 127.0.0.1:4222"
