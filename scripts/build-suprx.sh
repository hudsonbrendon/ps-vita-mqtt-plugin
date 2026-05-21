#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

IMAGE="ps-vita-mqtt-build:local"

docker build --platform=linux/amd64 -t "$IMAGE" "$ROOT/docker"
docker run --rm --platform=linux/amd64 -v "$ROOT:/work" -w /work "$IMAGE" make suprx
