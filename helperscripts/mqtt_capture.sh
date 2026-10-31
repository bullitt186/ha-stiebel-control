#!/bin/bash
# Long-term capture of everything the heat pump firmware publishes (states + decoded CAN frames on
# heatingpump/debug, incl. raw values), one line per message: "<ISO time> <topic> <payload>".
# Scan results are excluded (see register_probe.sh).
#
# Usage: MQTT_PASSWORD=... helperscripts/mqtt_capture.sh <outfile> [broker] [user]
# Password defaults to mqtt_password from esphome/secrets.yaml.
set -euo pipefail
OUT=${1:?outfile}
BROKER=${2:-192.168.30.10}
USER_=${3:-iotuser}
HERE=$(cd "$(dirname "$0")/.." && pwd)
PW=${MQTT_PASSWORD:-$(grep mqtt_password "$HERE/esphome/secrets.yaml" | sed 's/mqtt_password: *//' | tr -d '"')}
exec mosquitto_sub -h "$BROKER" -u "$USER_" --pw "$PW" -t 'heatingpump/#' -T 'heatingpump/scan/#' -R \
    -F '%I %t %p' >> "$OUT"
