#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

IMAGE="ps-vita-mqtt-build:local"

docker build -t "$IMAGE" "$ROOT/docker"
docker run --rm -v "$ROOT:/work" -w /work "$IMAGE" make skprx
