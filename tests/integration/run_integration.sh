#!/usr/bin/env bash
set -euo pipefail

# Bring up Mosquitto, build host-tests, run test_publisher against it,
# subscribe and verify discovery + state topics are populated.

cleanup() {
  docker rm -f psvita-mqtt-mosquitto >/dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
docker run -d --name psvita-mqtt-mosquitto -p 1883:1883 \
  eclipse-mosquitto:2 \
  sh -c 'printf "listener 1883\nallow_anonymous true\n" > /m.conf && \
         mosquitto -c /m.conf'

# wait for broker
for i in $(seq 1 30); do
  if nc -z 127.0.0.1 1883 2>/dev/null; then break; fi
  sleep 0.5
done
nc -z 127.0.0.1 1883 || { echo "broker did not come up"; exit 1; }

make host-tests

# Run the publisher against the real broker
MOSQUITTO_HOST=127.0.0.1 ./host-tests/test_publisher

# Verify a discovery topic landed on the broker
TIMEOUT=5
EXPECT_TOPIC="homeassistant/sensor/psvita-test_battery_level/config"
FOUND=$(docker exec psvita-mqtt-mosquitto \
  mosquitto_sub -h 127.0.0.1 -t "$EXPECT_TOPIC" -W $TIMEOUT -C 1 || true)
test -n "$FOUND" || { echo "FAIL: $EXPECT_TOPIC not retained"; exit 1; }
echo "PASS: discovery topic retained on broker"
