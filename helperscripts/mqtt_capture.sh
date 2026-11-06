#!/bin/bash
# Long-term capture of everything the heat pump firmware publishes (states + decoded CAN frames on
# heatingpump/debug, incl. raw values), one line per message: "<ISO time> <topic> <payload>".
# Scan results are excluded (see register_probe.sh).
#
# Usage: MQTT_PASSWORD=... helperscripts/mqtt_capture.sh <outfile> [broker] [user]
# Broker and user fall back to MQTT_BROKER and MQTT_USER, then to esphome/secrets.yaml; password
# defaults to mqtt_password from esphome/secrets.yaml.
set -euo pipefail
OUT=${1:?outfile}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BROKER=${2:-${MQTT_BROKER:-$(grep '^mqtt_broker:' "$HERE/esphome/secrets.yaml" | sed 's/mqtt_broker: *//; s/ *#.*//' | tr -d '"' || true)}}
: "${BROKER:?no broker: pass it as an argument, set MQTT_BROKER or add mqtt_broker to esphome/secrets.yaml}"
USER_=${3:-${MQTT_USER:-$(grep '^mqtt_username:' "$HERE/esphome/secrets.yaml" | sed 's/mqtt_username: *//; s/ *#.*//' | tr -d '"' || true)}}
: "${USER_:?no user: pass it as an argument, set MQTT_USER or add mqtt_username to esphome/secrets.yaml}"
PW=${MQTT_PASSWORD:-$(grep mqtt_password "$HERE/esphome/secrets.yaml" | sed 's/mqtt_password: *//' | tr -d '"')}
exec mosquitto_sub -h "$BROKER" -u "$USER_" --pw "$PW" -t 'heatingpump/#' -T 'heatingpump/scan/#' -R \
    -F '%I %t %p' >> "$OUT"
