#!/bin/bash
# Read-only register probe: range-scans status registers once a minute (pumps, mixer, relays, buffer
# setpoint) via the firmware's bus scan and logs every answer: "<ISO time> <topic> <payload>".
# Each round pauses normal polling for ~14 s. Never writes to the heat pump.
#
# Usage: MQTT_PASSWORD=... helperscripts/register_probe.sh <outfile> [rounds] [broker] [user]
# Broker and user fall back to MQTT_BROKER and MQTT_USER, then to esphome/secrets.yaml.
set -euo pipefail
OUT=${1:?outfile}
N=${2:-360}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BROKER=${3:-${MQTT_BROKER:-$(grep '^mqtt_broker:' "$HERE/esphome/secrets.yaml" | sed 's/mqtt_broker: *//; s/ *#.*//' | tr -d '"' || true)}}
: "${BROKER:?no broker: pass it as an argument, set MQTT_BROKER or add mqtt_broker to esphome/secrets.yaml}"
USER_=${4:-${MQTT_USER:-$(grep '^mqtt_username:' "$HERE/esphome/secrets.yaml" | sed 's/mqtt_username: *//; s/ *#.*//' | tr -d '"' || true)}}
: "${USER_:?no user: pass it as an argument, set MQTT_USER or add mqtt_username to esphome/secrets.yaml}"
PW=${MQTT_PASSWORD:-$(grep mqtt_password "$HERE/esphome/secrets.yaml" | sed 's/mqtt_password: *//' | tr -d '"')}
H=(-h "$BROKER" -u "$USER_" --pw "$PW")
mosquitto_sub "${H[@]}" -t 'heatingpump/scan/#' -R -F '%I %t %p' >> "$OUT" &
SUB=$!
trap 'kill $SUB' EXIT
for i in $(seq 1 "$N"); do
  t0=$(date +%s)
  mosquitto_pub "${H[@]}" -t heatingpump/scan/start -m 'KESSEL 0x0050 0x0066'; sleep 6
  mosquitto_pub "${H[@]}" -t heatingpump/scan/start -m 'KESSEL 0x01d2 0x01dd'; sleep 4
  mosquitto_pub "${H[@]}" -t heatingpump/scan/start -m 'MANAGER 0x0a20 0x0a20'; sleep 2
  mosquitto_pub "${H[@]}" -t heatingpump/scan/start -m 'HEIZMODUL 0x0176 0x0176'; sleep 2
  while [ $(( $(date +%s) - t0 )) -lt 60 ]; do sleep 1; done
done
